# pico1541 - Pi1541 Raspberry Pico 2 (W) support 

This is an attempt to make Pi1541 available on a Pico 2

## Build
I use VSCode and its pico extension - this installs the Pico SDK in ~/.pico_sdk.
As there's the need to have some files being read from an SDCard, one needs the respective setup. I use an SPI connected SDCard reader in a more or less standard SPI configuration

In order to build, you have to checkout the driver library before.
```
$ cd <...checkout-path...>/pottendo-Pi1541/pico1541
$ mkdir lib ; cd lib
$ git clone --recurse-submodules https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git no-OS-FatFs/
$ cd ..
$ code .
```
you should now be able to build and run.

## Prerequisites
Your SDCard has to be formated with FAT32 and must be populated with the usual Pi1541 files:
- a valid options.txt, active `headLess = 1`, there's monitor display support yet
- matching floppy ROMs
- a directory 1541 populated with some .d64 images, most noticable one FB.d64
- maybe more

## Hardware setup
The following pins have been assigned to the pico setup:
| Line | GPIO Number  |
|:----|:-------------:|
|PIGPIO_ATN | 10|
|PIGPIO_CLOCK | 11 |
|	PIGPIO_DATA |12|
|	PIGPIO_RESET |13|
|	PIGPIO_SRQ |14|
|	PIGPIO_IN_BUTTON1 | 5|
|	PIGPIO_IN_BUTTON2 |6|
|	PIGPIO_IN_BUTTON3 | 7|
|	PIGPIO_IN_BUTTON4 | 8|
|	PIGPIO_IN_BUTTON5 | 9|
|	PIGPIO_OUT_SOUND | 14|
|	PIGPIO_OUT_LED   | 15|
SDCard SPI0:
|   sck_gpio |2|
|   mosi_gpio |3|
|   miso_gpio |4|
|   ss_gpio | 1|

XXX todo assignment & code for split IEC lines HW.

## Status
The code has't yet been verified to work with a real C64 as the complete I/O isn't tested as of now.
It's now unsolved how to overcome the memory limit when a diskimage is being read from the sdcard.

The code is planned to remain compatible to all versions supported by Pi1541.

## TODO:
- *implement I/O -> validate that pico 2 is fast enough for Pi1541*
- fix `ff.h` and `ffconfig.h` for legacy build
- activate UI: buttons & LCD display
