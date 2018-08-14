
/*
 ESP8266-based clock that uses NTP to stay in sync, and a DHT11/22
 to also display the temperature and humidity.

| ILI9341 SPI TFT | NodeMCU |
|-----------------|---------|
| VCC             | 3.3V    |
| GND             | GND     |
| CS              | D2      |
| REST            | RST     |
| DC              | D1      |
| SDI/MOSI        | D7      |
| SCK             | D5      |
| LED             | 3.3V    |
| SDO/MISO        | D6      |


| DHT11 | NodeMCU |
|-------|---------|
| VCC   | 3.3V    |
| Data  | D0      |
| GND   | GND     |


 Based on clock sketch by Gilchrist 6/2/2014 1.0

A few colour codes:

code	color
0x0000	Black
0xFFFF	White
0xBDF7	Light Gray
0x7BEF	Dark Gray
0xF800	Red
0xFFE0	Yellow
0xFBE0	Orange
0x79E0	Brown
0x7E0	Green
0x7FF	Cyan
0x1F	Blue
0xF81F	Pink

 */
#include <DHTesp.h>

#include "passwords.h"  // Define WIFI_PASS and WIFI_SSID here

#define DHT11_PIN D0

DHTesp dht;

const int INTERVAL_DHT = 30;

#include <TFT_eSPI.h> // Hardware-specific library https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>

#include <Timezone.h>    // https://github.com/JChristensen/Timezone


// Time variables

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);

TimeChangeRule *tcr;  


// Wifi variables

bool connectionWasAlive = false;

ESP8266WiFiMulti wifiMulti;      // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

WiFiUDP UDP;                     // Create an instance of the WiFiUDP class to send and receive

IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "hora.roa.es";

const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message

byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets


unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;


// TFT variables

#define TFT_GREY 0x5AEB
#define TFT_DARK_GREY 0x7BEF

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

#define FONT_SIZE 7

// Clock variables

uint8_t hh = 0, mm = 0, ss = 0;

byte omm = 99, oss = 99;
byte xcolon = 0, xsecs = 0;
unsigned int colour = 0;

uint32_t targetTime = 0;                    // for next 1 second timeout



// Other variables

int counter = 0;

void updateTime() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // If a minute has passed since last NTP request
    prevNTP = currentMillis;
    Serial.println("\r\nSending NTP request ...");
    sendNTPpacket(timeServerIP);               // Send an NTP request
  }

  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
  } else if ((currentMillis - lastNTPResponse) > 3600000) {
    Serial.println("More than 1 hour since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }


  time_t t = CE.toLocal(now(), &tcr);

  mm = minute(t);
  ss = second(t);
  hh = hour(t);
  
  if ( ss != oss ) { // If a second has passed since last print
    Serial.printf("\rLocal time:\t%d:%d:%d   ", hh, omm, oss);
  }  
}


void drawTime(){
      int xpos = 20;
    int ypos = 65; // Top left corner ot clock text, about half way down
    int ysecs = ypos + 24;

    if (omm != mm) { // Redraw hours and minutes time every minute
      omm = mm;
      // Draw hours and minutes
      if (hh < 10) xpos += tft.drawChar('0', xpos, ypos, FONT_SIZE); // Add hours leading zero for 24 hr clock
      xpos += tft.drawNumber(hh, xpos, ypos, FONT_SIZE);             // Draw hours
      xcolon = xpos; // Save colon coord for later to flash on/off later
      xpos += tft.drawChar(':', xpos, ypos - 8, FONT_SIZE);
      if (mm < 10) xpos += tft.drawChar('0', xpos, ypos, FONT_SIZE); // Add minutes leading zero
      xpos += tft.drawNumber(mm, xpos, ypos, FONT_SIZE);             // Draw minutes
      xsecs = xpos; // Sae seconds 'x' position for later display updates
    }
    if (oss != ss) { // Redraw seconds time every second
      oss = ss;
      xpos = xsecs;

      if (ss % 2) { // Flash the colons on/off
        tft.setTextColor(0x39C4, TFT_BLACK);        // Set colour to grey to dim colon
        tft.drawChar(':', xcolon, ypos - 8, FONT_SIZE);     // Hour:minute colon
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);    // Set colour back to yellow
      }
      else {
        tft.drawChar(':', xcolon, ypos - FONT_SIZE, FONT_SIZE);     // Hour:minute colon
        }
    }
}


uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  setTime(UNIXTime);
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}


void monitorWiFi() {
  if (wifiMulti.run() != WL_CONNECTED)
  {
    if (connectionWasAlive)
    {
      connectionWasAlive = false;
      Serial.print("Looking for WiFi ");
    }
    Serial.print(".");
  }
  else if (connectionWasAlive == false)
  {
    connectionWasAlive = true;
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());             // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
    Serial.println("\r\n"); 
   }
}

void startWiFi() { // Try to connect to some given access points. Then wait for a connection
  wifiMulti.addAP(WIFI_SSID, WIFI_PASS);   // add Wi-Fi networks you want to connect to

  while (wifiMulti.run() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    delay(250);
    monitorWiFi();
  }
  Serial.println("\r\n");
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
}


void printTemperature()
{

  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
  Serial.print("\t\t");
  Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);

  int space = 16;
  int pos = 24;
  int font = 4;
  int posy = 180;
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);

  pos += tft.drawString("T: ", pos, posy+20, font);  

  pos += tft.drawNumber(temperature, pos+space, posy, 7) + 2*space;

  pos += tft.drawString("C", pos, posy, font) + space;     
 
  pos += tft.drawString("H: ", pos, posy+20, font);

  pos += tft.drawNumber(humidity, pos + space, posy, 7);    
  pos += tft.drawChar('%', pos + space, posy, font);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);

}


void setup(void) {
  dht.setup(DHT11_PIN, DHTesp::DHT11); // Connect DHT sensor to GPIO 17

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  targetTime = millis() + 1000;
   
  Serial.begin(115200);          // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi();                   // Try to connect to some given access points. Then wait for a connection

  startUDP();

  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  
  Serial.println("\r\nSending NTP request ...");
  sendNTPpacket(timeServerIP);  

}


void loop() {

  if (targetTime < millis()) {
    targetTime = millis() + 1000;
    
    monitorWiFi();
    
    if ( (counter % INTERVAL_DHT) == 0){
      counter = 0;
      printTemperature();       
    }
    counter = counter+1;

    updateTime();


    // Update digital time
    drawTime();

  }
}
