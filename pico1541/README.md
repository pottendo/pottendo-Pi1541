# pico1541 Pico 2 support 

This is an attempt to make Pi1541 available on a Pico 2

## build
I use VSCode and it's pico extension. 
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

## prerequisites
Your SDCard has to be formated with FAT32 and must be populated with the usual Pi1541 files:
- a valid options.txt # active headLess = 1, there's no display support yet
- matchin floppy ROMs
- a directory 1541 populated with some .d64 images, most noticable one FB.d64
- maybe more
## Status
The code has't yet been verified to work with a real C64 as the complete I/O isn't yet implemented.
It's been verified that the startup works and the emulation waits for a reset.

