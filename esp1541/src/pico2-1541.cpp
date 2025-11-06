#include <Arduino.h>
#include <stdio.h>
#include "pico2.h"
#include "../../src/Screen.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "hw_config.h"

extern "C" void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags);
extern void list_directory(const char *path);
#define MAX_PATH 255
static char cwd[MAX_PATH + 1]="";

void _DEBUG(const char* fmt, ...) 
{
    char t[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(t, 256, fmt, args);
    Serial.println(t);
    va_end(args);
}

void plfio_showstat(void)
{
    Serial.printf("Chip model: %s\n", rp2040.getChipID());
    Serial.printf("Free heap: %d/%d\n", rp2040.getFreeHeap(), rp2040.getTotalHeap());
    Serial.printf("Free PSRAM: %d/%d\n", rp2040.getFreePSRAMHeap(), rp2040.getTotalPSRAMHeap());
}

int pico2_initSD(void)
{
    printf("%s: sucessfully initialized SD card!\n", __FUNCTION__);
    strcpy(cwd, "/");
    return 0;
}

void pico2_setup(void)
{
    Serial.begin(115200);
    int _i = 7;
    while (_i--)
    {
        printf("counting %s: - %d\n", __FUNCTION__, _i);
        sleep_ms(1000);
        fflush(stdout);
    }
    plfio_showstat();
    printf("%s: initializing SD card...\n", __FUNCTION__);
    pico2_initSD();
    FATFS fileSystemSD;
    FRESULT fr = f_mount(&fileSystemSD, "SD:", 1);
    if (FR_OK != fr) {
       	Serial.printf("f_mount error: (%d)\n", fr);
    }
    printf("%s: listing root directory:\n", __FUNCTION__);
	list_directory("/");
    fflush(stdout);
    printf("%s: setting CPU frequency to 250MHz\n", __FUNCTION__);
    set_sys_clock_khz(250000, true);
    kernel_main(0, 0, 0);
}

void pico2_loop(void)
{
    printf("%s: entering main loop...\n", __FUNCTION__);
    while (true) {
        Serial.printf("pico2 Pi1541 done!\n");
        sleep_ms(10000);
    }
}
void not_implemented(const char *fn)
{
    Serial.printf("%s for Pico2\n", fn);
}

extern "C" {
void SetACTLed(int v) 
{ 
    Serial.printf("%s: %s (%d)\n", __FUNCTION__, (v ? "on" : "off"), v);
}

void usDelay(unsigned nMicroSeconds)
{
    sleep_us(nMicroSeconds);
}
} /* extern "C" */

void InitialiseLCD(void)
{
    not_implemented(__FUNCTION__);
}

void reboot_now(void)
{
    not_implemented(__FUNCTION__);
}

FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn)	/* Get volume label */
{
    not_implemented(__FUNCTION__);
    return FR_DISK_ERR;
}

FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask)			/* Change attribute of a file/dir */
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

