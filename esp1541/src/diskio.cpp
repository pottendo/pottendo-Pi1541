#include <Arduino.h>
/*
  SD card basic file example
 
 This example shows how to create and destroy an SD card file 	
 The circuit:
 * SD card attached to SPI bus as follows:
 ** UNO:  MOSI - pin 11, MISO - pin 12, CLK - pin 13, CS - pin 4 (CS pin can be changed)
  and pin #10 (SS) must be an output
 ** Mega:  MOSI - pin 51, MISO - pin 50, CLK - pin 52, CS - pin 4 (CS pin can be changed)
  and pin #52 (SS) must be an output
 ** Leonardo: Connect to hardware SPI via the ICSP header

 created   Nov 2010 by David A. Mellis
 modified 9 Apr 2012 by Tom Igoe
 modified 13 June 2012 by Limor Fried
 
 This example code is in the public domain.
 	 
 */
#include "diskio.h"

#define SS 25
#include <SPI.h>
#include <mySD.h>

File root;

void printDirectory(File dir, int numTabs) {
  // Begin at the start of the directory
  dir.rewindDirectory();
  
  while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');   // we'll have a nice indentation
     }
     // Print the 8.3 name
     Serial.print(entry.name());
     // Recurse for directories, otherwise print the file size
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

void SD_setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  Serial.print("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on Arduino Uno boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
  
  pinMode(SS, OUTPUT);

  if (!SD.begin(25,MOSI,MISO,SCK)){
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  root = SD.open("/");
  
  printDirectory(root, 0);
  
  Serial.println("done!");
}

DSTATUS disk_initialize (BYTE pdrv)
{ 
  not_implemented();
  return -1;
}
DSTATUS disk_status (BYTE pdrv)
{ 
  not_implemented();
  return -1;
}
DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
{ 
  not_implemented();
  return RES_PARERR;
}
DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
{ 
  not_implemented();
  return RES_PARERR;
}
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff);
{ 
  not_implemented();
  return RES_PARERR;
}
