ESP8266-based clock that uses NTP to stay in sync, and a DHT11/22 to also display the temperature and humidity.

WORK IN PROGRESS.

# Connections

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

# Arduino ESP8266 support

Boards manager link: http://arduino.esp8266.com/stable/package_esp8266com_index.json

# Libraries

Some of them are available via the Arduino library manager

* https://github.com/PaulStoffregen/Time
* https://github.com/JChristensen/Timezone
* https://github.com/Bodmer/TFT_eSPI
* https://github.com/beegee-tokyo/DHTesp
