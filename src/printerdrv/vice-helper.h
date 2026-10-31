#ifndef VICE_HELPER_H
#define VICE_HELPER_H

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#if !defined (__CIRCLE__)
void vice_helper_debug_log(const char *format, ...);
#define DEBUG_LOG(...) vice_helper_debug_log(__VA_ARGS__)
#else
void klog(const char *fmt,...);
#define DEBUG_LOG(...) klog(__VA_ARGS__)
#endif

typedef struct vice_helper_file_s vice_helper_file_t;

#define VICE_HELPER_EOF (-1)

vice_helper_file_t *vice_helper_file_open(const char *path, const char *mode);
vice_helper_file_t *vice_helper_stdin(void);
vice_helper_file_t *vice_helper_stdout(void);
vice_helper_file_t *vice_helper_stderr(void);
size_t vice_helper_file_read(void *buffer, size_t size, size_t count,
                             vice_helper_file_t *file);
size_t vice_helper_file_write(const void *buffer, size_t size, size_t count,
                              vice_helper_file_t *file);
int vice_helper_file_close(vice_helper_file_t *file);
int vice_helper_file_putc(int character, vice_helper_file_t *file);
int vice_helper_file_puts(const char *string, vice_helper_file_t *file);
int vice_helper_file_vprintf(vice_helper_file_t *file, const char *format,
                             va_list args);
int vice_helper_file_printf(vice_helper_file_t *file, const char *format, ...);
int vice_helper_puts(const char *string);
int vice_helper_file_flush(vice_helper_file_t *file);
long vice_helper_file_tell(vice_helper_file_t *file);
int vice_helper_file_error(vice_helper_file_t *file);
int vice_helper_snprintf(char *buffer, size_t size, const char *format, ...);

typedef signed int log_t;
#define LOG_DEFAULT ((log_t)-1)

typedef struct palette_entry_s {
    char *name;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} palette_entry_t;

typedef struct palette_s {
    unsigned int num_entries;
    palette_entry_t *entries;
} palette_t;

typedef struct output_parameter_s {
    unsigned int maxcol;
    unsigned int maxrow;
    unsigned int dpi_x;
    unsigned int dpi_y;
    palette_t *palette;
} output_parameter_t;

typedef struct driver_select_s {
    char *drv_name;
    char *ui_name;
    int (*drv_open)(unsigned int prnr, unsigned int secondary);
    void (*drv_close)(unsigned int prnr, unsigned int secondary);
    int (*drv_putc)(unsigned int prnr, unsigned int secondary, uint8_t b);
    int (*drv_getc)(unsigned int prnr, unsigned int secondary, uint8_t *b);
    int (*drv_flush)(unsigned int prnr, unsigned int secondary);
    int (*drv_formfeed)(unsigned int prnr);
    int (*drv_select)(unsigned int prnr);
    bool printer;
    bool plotter;
    bool iec;
    bool ieee488;
    bool userport;
    bool text;
    bool graphics;
} driver_select_t;

#define DRIVER_FIRST_OPEN 0xffff
#define DRIVER_LAST_CLOSE 0xffff
#define NUM_OUTPUT_SELECT 4
#define PRINTER_USERPORT 3
#define USERPORT_DEVICE_PRINTER 1
#define MPS801_ROM_SIZE (4 * 1024)
#define MPS802_ROM_SIZE (8 * 1024)
#define MPS803_ROM_SIZE (4 * 1024)
#define C2022_ROM_SIZE (4 * 1024)
#define C4023_ROM_SIZE (8 * 1024)
#define C8023_ROM_SIZE (8 * 1024)
#define MPS801_ROM_NAME "mps801.bin"
#define MPS802_ROM_NAME "cbm1526-325341-08.bin"
#define C2022_ROM_NAME "2022-901472-04.bin"
#define C4023_ROM_NAME "4023-325360-02.bin"
#define C8023_ROM_NAME "8023-325320-02.bin"
#define NL10_ROM_NAME "nl10.bin"
#define MPS803_ROM_NAME "mps803-D7811G-111-U32053A.bin"

#define OUTPUT_PIXEL_BLACK '*'
#define OUTPUT_PIXEL_WHITE ' '
#define OUTPUT_PIXEL_RED 'R'
#define OUTPUT_PIXEL_GREEN 'G'
#define OUTPUT_PIXEL_BLUE 'B'
#define OUTPUT_NEWLINE '\n'

#define VICE_HELPER_GFX_MAX_WIDTH 1900

void driver_select_register(driver_select_t *driver_select);
void vice_helper_select_driver(const char *driver_name);
driver_select_t *vice_helper_driver(void);

int output_select_open(unsigned int prnr, output_parameter_t *output_parameter);
void output_select_close(unsigned int prnr);
int output_select_putc(unsigned int prnr, uint8_t b);
int output_select_getc(unsigned int prnr, uint8_t *b);
int output_select_flush(unsigned int prnr);
int output_select_formfeed(unsigned int prnr);

void vice_helper_set_output_options(int stdout_enabled, int gfx_enabled);
int vice_helper_output_error(void);
const uint8_t *vice_helper_gfx_pixels(void);
size_t vice_helper_gfx_rows(void);
size_t vice_helper_gfx_width(void);
unsigned int vice_helper_gfx_dpi_x(void);
unsigned int vice_helper_gfx_dpi_y(void);
void vice_helper_free_gfx_pixels(void);

void *lib_malloc(size_t size);
void *lib_calloc(size_t count, size_t size);
void *lib_malloc_pinpoint(size_t size, const char *name, unsigned int line);
void lib_free(void *ptr);
void lib_free_pinpoint(void *ptr, const char *name, unsigned int line);

log_t log_open(const char *id);
int log_warning(log_t log, const char *format, ...);
int log_error(log_t log, const char *format, ...);
int log_message(log_t log, const char *format, ...);

palette_t *palette_create(unsigned int num_entries,
                          const char *entry_names[]);
void palette_free(palette_t *palette);
int palette_load(const char *file_name, const char *subpath,
                 palette_t *palette_return);

int sysfile_load(const char *name, const char *subpath, uint8_t *dest,
                 int minsize, int maxsize);
int userport_get_device(void);

#endif
