#ifndef __PICO2_H__
#define __PICO2_H__

#define HAS_40PINS
#include <stdint.h>
#include <stdarg.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef bool boolean;

extern "C" {
    void SetACTLed(int v);
    void usDelay(unsigned nMicroSeconds);
}
void reboot_now(void);
void Reboot_Pi(void);
void InitialiseLCD(void);
void not_implemented(const char *fn);
#endif