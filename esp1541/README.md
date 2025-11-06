# pico1541 - Pi1541 ESP32/PSRAM support 

This is an attempt to make Pi1541 available on a esp32/PSRAM

## Build
I use VSCode and its platformIO extension.
As there's the need to have some files being read from an SDCard, one needs the respective setup. I use an SPI connected SDCard reader in a more or less standard SPI configuration

In order to build, you have to checkout the driver library before.
```
$ cd <...checkout-path...>/pottendo-Pi1541/esp1541
$ code .
```
you should now be able to build and run from within code/platformIO

## Prerequisites
Pi1541 needs >1MB memory - You need a respective ESP32/Pico2 MCU populated with PSRAM or other RAM expansions, compared to the typical stock versions of these MCUs which won't provide sufficient RAM.

A Pico2 with 8MB PSRam: https://shop.pimoroni.com/products/pimoroni-pico-plus-2-w?variant=42182811942995
A ESP32 with 4MB PSRam: https://docs.pycom.io/gitbook/assets/specsheets/Pycom_002_Specsheets_GPy_v2.pdf

Your SDCard has to be formated with FAT32 and must be populated with the usual Pi1541 files:
- a valid options.txt, add `headLess = 1`, there's no monitor display support yet
- matching floppy ROMs
- a directory 1541 populated with some .d64 images, most noticable one FB.d64
- maybe more

## Hardware setup
The following pins have been assigned to the esp setup:
| Line | GPIO Number  |
|:----|:-------------:|
|PIGPIO_ATN | 3|
|PIGPIO_CLOCK | 15 |
|	PIGPIO_DATA |4|
|	PIGPIO_RESET |19|
|	PIGPIO_SRQ (not used)|14|
|	PIGPIO_IN_BUTTON1 | 5|
|	PIGPIO_IN_BUTTON2 |6|
|	PIGPIO_IN_BUTTON3 | 7|
|	PIGPIO_IN_BUTTON4 | 8|
|	PIGPIO_IN_BUTTON5 | 9|
|	PIGPIO_OUT_SOUND | 26|
|	PIGPIO_OUT_LED   | 14|

SDCard SPI0: pin description here XXXTBD

XXX todo assignment & code for split IEC lines HW.

The following pins have been assigned to the pico2 setup:
| Line | GPIO Number  |
|:----|:-------------:|
| PIGPIO_ATN | 10|
| PIGPIO_CLOCK | 11|
| PIGPIO_DATA | 12|
| PIGPIO_RESET | 13|
| PIGPIO_SRQ  (not used)| 14|
| PIGPIO_IN_BUTTON1 | 16|
| PIGPIO_IN_BUTTON2 | 17|
| PIGPIO_IN_BUTTON3 | 18|
| PIGPIO_IN_BUTTON4 | 19|
| PIGPIO_IN_BUTTON5 | 20|
| PIGPIO_OUT_SOUND |14|
| PIGPIO_OUT_LED   | 15|
| SDCard CS | 1|
| SDCard CLK | 2|
| SDCard MOSI | 3|
| SDCard MISO | 4|
| SDCard 3.3V | 3.3V| 
| SDCard GND | GND |


## Status
This is a PoC to get Pi1541 running on smaller uControllers. The ESP32 CPU + I/O proved to be *too slow* to meet the real-time requirements for Pi1541 - therefore won't work.
The code can be used as a further porting base to more powerful uControllers - similar to Pico2.

The code is planned to remain compatible to all versions supported by Pi1541.

Note that Pico2 is substantially overclocked (250Mhz) - use at your own risk!

## TODO:
- activate UI: buttons & LCD display
- Fix pico build for FatFS - currently needs some adjustment in fconf.h in the lib dir after checkout
