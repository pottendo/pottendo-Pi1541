#include <Arduino.h>
#include "esp32.h"
#include <SD.h>
#include <ff.h>
#include <LiteLED.h>
extern "C" void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags);
#define LED_TYPE        LED_STRIP_WS2812
#define LED_TYPE_IS_RGBW 0   // if the LED is an RGBW type, change the 0 to 1
#define LED_GPIO GPIO_NUM_0     // change this number to be the GPIO pin connected to the LED
#define LED_BRIGHT 20   // sets how bright the LED is. O is off; 255 is burn your eyeballs out (not recommended)

// pick the colour you want from the list here and change it in setup()
static const crgb_t L_RED = 0xff0000;
static const crgb_t L_GREEN = 0x00ff00;
static const crgb_t L_BLUE = 0x0000ff;
static const crgb_t L_WHITE = 0xe0e0e0;
LiteLED myLED( LED_TYPE, LED_TYPE_IS_RGBW );    // create the LiteLED object; we're calling it "myLED"

void plfio_showstat(void)
{
    printf("Chip model: %s, %dMHz, %d cores\n", ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getChipCores());
    printf("Free heap: %d/%d %d max block\n", ESP.getFreeHeap(), ESP.getHeapSize(),ESP.getMaxAllocHeap());
    printf("Free PSRAM: %d/%d %d max block\n",ESP.getFreePsram(), ESP.getPsramSize(),ESP.getMaxAllocPsram());
}

void esp32_setup(void)
{
    Serial.begin(115200);
    printf("\n\n\nXXXXXXXXXXXXXXXXXXXXXXXXXXXX\nChip model: %s, %dMHz, %d cores\n", ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getChipCores());
    printf("Free heap: %d/%d %d max block\n", ESP.getFreeHeap(), ESP.getHeapSize(),ESP.getMaxAllocHeap());
    printf("Free PSRAM: %d/%d %d max block\n",ESP.getFreePsram(), ESP.getPsramSize(),ESP.getMaxAllocPsram());
    myLED.begin( LED_GPIO, 1 );         // initialze the myLED object. Here we have 1 LED attached to the LED_GPIO pin
    myLED.brightness( LED_BRIGHT );     // set the LED photon intensity level
    myLED.setPixel( 0, L_RED, 1 );    // set the LED colour and show it
}

void esp32_loop(void)
{
    printf("%s: entering main loop...\n", __FUNCTION__);
    kernel_main(0, 0, 0);
    while (true) {
        printf("esp32 Pi1541 done!\n");
        sleep(1000);
    }
}

void not_implemented(const char *fn)
{
    fprintf(stderr, "%s %s\n", fn, __FUNCTION__);
}

/* --------- PICO2 specific Pi1541 functions here --------- */
extern "C" {
void SetACTLed(int v) 
{ 
    printf("%s: %s (%d)\n", __FUNCTION__, (v ? "on" : "off"), v);
    if (v)
        myLED.brightness( LED_BRIGHT, 1 );
    else
        myLED.brightness( 0, 1 );
}

void usDelay(unsigned nMicroSeconds)
{
    usleep(nMicroSeconds);
}
} /* extern "C" */

void reboot_now(void)
{
    not_implemented(__FUNCTION__);
}

void InitialiseLCD(void)
{
    not_implemented(__FUNCTION__);
}
uint64_t get_ticks(void)
{
    return micros();
}

extern void list_directory(const char *p);

#define MAX_PATH 127
static char cwd[MAX_PATH + 1];

int esp32_initSD(void)
{
    if(!SD.begin(25)){
        Serial.println("Card Mount Failed");
        return -1;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return -2;
    }
    printf("%s: sucessfully initialized SD card!\n", __FUNCTION__);
    strcpy(cwd, "/");
    return 0;
}

FRESULT f_findnext (
	DIR* dp,		/* Pointer to the open directory object */
	FILINFO* fno	/* Pointer to the file information structure */
)
{
	FRESULT res;


	for (;;) {
		res = f_readdir(dp, fno);		/* Get a directory item */
		if (res != FR_OK || !fno || !fno->fname[0]) break;	/* Terminate if any error or end of directory */
		//if (pattern_matching(dp->pat, fno->fname, 0, 0)) break;		/* Test for the file name */
#if _USE_LFN != 0 && _USE_FIND == 2
		if (pattern_matching(dp->pat, fno->altname, 0, 0)) break;	/* Test for alternative name if exist */
#endif
	}
	return res;
}

/*-----------------------------------------------------------------------*/
/* Find First File                                                       */
/*-----------------------------------------------------------------------*/

FRESULT f_findfirst (
	DIR* dp,				/* Pointer to the blank directory object */
	FILINFO* fno,			/* Pointer to the file information structure */
	const TCHAR* path,		/* Pointer to the directory to open */
	const TCHAR* pattern	/* Pointer to the matching pattern */
)
{
	FRESULT res;


	//dp->pat = pattern;		/* Save pointer to pattern string */
	res = f_opendir(dp, path);		/* Open the target directory */
	if (res == FR_OK) {
		res = f_findnext(dp, fno);	/* Find the first item */
	}
	return res;
}

FRESULT f_chdir (const TCHAR* path)								/* Change current directory */
{
    printf("chdir to %s\n", path);
    strncpy(cwd, path, MAX_PATH);
    return FR_OK;
}

FRESULT f_getcwd (TCHAR* buff, UINT len)							/* Get current directory */
{
    strncpy(buff, cwd, len);
    return FR_OK;
}

FRESULT f_chdrive (const TCHAR* path)								/* Change current drive */
{
    not_implemented(__FUNCTION__);
    return FR_DISK_ERR;
}

FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn)	/* Get volume label */
{
    not_implemented(__FUNCTION__);
    return FR_DISK_ERR;
}

#undef f_open
FRESULT _f_open (FIL* fp, const TCHAR* path, BYTE mode)
{
    if (!path)
        return FR_EXIST;
    static char xpath[MAX_PATH + 1];
    xpath[0] = '\0';
    if (path[0] != '/')
    {
        snprintf(xpath, MAX_PATH, "%s/", cwd);
    }
    strncat(xpath, path, MAX_PATH - strlen(path));

    FRESULT fr = f_open (fp, xpath, mode);	
    printf("%s: %s - open: %d\n", __FUNCTION__, xpath, fr);
    
    return fr;
}

#undef f_opendir
FRESULT _f_opendir(DIR* dp, const TCHAR* path)
{
    printf("%s: path = %s, cwd = %s\n", __FUNCTION__, path, cwd);
    if (strcmp(".", path) == 0)
        return f_opendir(dp, cwd);
    return f_opendir(dp, path);
}

