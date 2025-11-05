#ifndef __ESP32_H__
#define __ESP32_H__
#include <stdint.h>
#include <stdarg.h>
#include <cstddef>
#include <esp32-hal-psram.h>
#include "driver/gpio.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#define HAS_40PINS

#if 0
typedef uint32_t WORD;
typedef uint32_t UINT;
typedef uint16_t WCHAR;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
#endif
#include <ff.h>
typedef FF_DIR DIR;

extern "C" {
    void SetACTLed(int v);
    void usDelay(unsigned nMicroSeconds);
}
int esp32_initSD(void);
void reboot_now(void);
void Reboot_Pi(void);
void InitialiseLCD(void);
void not_implemented(const char *fn);
uint64_t get_ticks(void);
void initDiskImage(void);
void plfio_showstat(void);

#define f_open _f_open
FRESULT _f_open (FIL* fp, const TCHAR* path, BYTE mode);
#define f_opendir _f_opendir
FRESULT _f_opendir (DIR* dp, const TCHAR* path);
#define __not_in_flash_func(a) a

#define pmalloc ps_malloc
#endif /* __ESP32_H__ */