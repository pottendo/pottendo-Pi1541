#ifndef types_h
#define types_h

#if defined (__CIRCLE__)

#include "circle-types.h"

#elif defined(__PICO2__)

#include "pico2.h"

#elif defined(ESP32)

#include "esp32.h"

#else

#include <stddef.h>
#include <uspi/types.h>
#include "integer.h"

typedef unsigned long long	u64;

typedef signed long long	s64;

#endif

typedef enum {
	LCD_UNKNOWN,
	LCD_1306_128x64,
	LCD_1306_128x32,
	LCD_1106_128x64,
	LCD_1107_128x128,
} LCD_MODEL;

typedef enum {
	EXIT_UNKNOWN,
	EXIT_RESET,
	EXIT_CD,
	EXIT_KEYBOARD,
	EXIT_AUTOLOAD
} EXIT_TYPE;

enum EmulatingMode
{
	IEC_COMMANDS,
	EMULATING_1541,
	EMULATING_1581,
	EMULATION_SHUTDOWN
};

#ifndef Max
#define Max(a,b)	(((a) > (b)) ? (a) : (b))
#endif

#ifndef Min
#define Min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

#endif
