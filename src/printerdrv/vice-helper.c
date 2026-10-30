#include "vice-helper.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mps803-rom.h"
#include "nl10-rom.h"

#define PNG_MAX_CONTENT_WIDTH VICE_HELPER_GFX_MAX_WIDTH

static driver_select_t selected_driver;
static int has_selected_driver;
static const char *selected_driver_name;
static char *raster;
static unsigned int raster_width;
static unsigned int raster_height;
static unsigned int raster_x;
static unsigned int raster_y;
static int raster_error;
static int stdout_enabled;
static int gfx_enabled;
static uint8_t *gfx_pixels;
static size_t gfx_width;
static size_t gfx_rows;
static size_t gfx_capacity;

static int start_raster(void)
{
    size_t raster_size = (size_t)raster_width * raster_height;

    raster = malloc(raster_size);
    if (raster == NULL) {
        raster_error = 1;
        return -1;
    }
    memset(raster, OUTPUT_PIXEL_WHITE, raster_size);
    raster_x = 0;
    raster_y = 0;
    return 0;
}

static int append_gfx_row(const char *pixels, size_t width)
{
    size_t new_capacity;
    uint8_t *new_pixels;

    if (gfx_rows == gfx_capacity) {
        if (gfx_capacity > SIZE_MAX / 2 / gfx_width) {
            return -1;
        }
        new_capacity = gfx_capacity == 0 ? 32 : gfx_capacity * 2;
        new_pixels = realloc(gfx_pixels,
                             new_capacity * gfx_width);
        if (new_pixels == NULL) {
            return -1;
        }
        gfx_pixels = new_pixels;
        gfx_capacity = new_capacity;
    }

    memset(gfx_pixels + gfx_rows * gfx_width,
           OUTPUT_PIXEL_WHITE, gfx_width);
    if (width > gfx_width) {
        width = gfx_width;
    }
    memcpy(gfx_pixels + gfx_rows * gfx_width, pixels, width);
    gfx_rows++;
    return 0;
}

static void render_raster(void)
{
    unsigned int x, y;
    unsigned int left = raster_width;
    unsigned int top = raster_height;
    unsigned int right = 0;
    unsigned int bottom = 0;

    if (raster == NULL) {
        return;
    }

    for (y = 0; y < raster_height; y++) {
        for (x = 0; x < raster_width; x++) {
            if (raster[y * raster_width + x] != OUTPUT_PIXEL_WHITE) {
                if (x < left) {
                    left = x;
                }
                if (x > right) {
                    right = x;
                }
                if (y < top) {
                    top = y;
                }
                if (y > bottom) {
                    bottom = y;
                }
            }
        }
    }

    if (left <= right && top <= bottom) {
        for (y = top; y <= bottom; y++) {
            unsigned int row_width = right - left + 1;

            if (gfx_enabled
                && append_gfx_row(raster + y * raster_width + left,
                                  row_width) < 0) {
                DEBUG_LOG("Cannot store raster for PNG output.");
                raster_error = 1;
                break;
            }
            if (stdout_enabled
                && (fwrite(raster + y * raster_width + left, 1,
                           row_width, stdout) != row_width
                    || putchar('\n') == EOF)) {
                raster_error = 1;
                break;
            }
        }
        if (stdout_enabled && fflush(stdout) != 0) {
            raster_error = 1;
        }
    }

    free(raster);
    raster = NULL;
}

void driver_select_register(driver_select_t *driver_select)
{
    if (selected_driver_name != NULL
        && strcmp(driver_select->drv_name, selected_driver_name) == 0) {
        selected_driver = *driver_select;
        has_selected_driver = 1;
    }
}

void vice_helper_select_driver(const char *driver_name)
{
    selected_driver_name = driver_name;
    has_selected_driver = 0;
}

driver_select_t *vice_helper_driver(void)
{
    return has_selected_driver ? &selected_driver : NULL;
}

int output_select_open(unsigned int prnr, output_parameter_t *output_parameter)
{
    (void)prnr;
    raster_width = output_parameter->maxcol;
    raster_height = output_parameter->maxrow;
    if (gfx_enabled) {
        if (gfx_width != 0 && gfx_width != raster_width && gfx_rows > 0) {
            DEBUG_LOG("Printer output dimensions changed during PNG capture.");
            raster_error = 1;
            return -1;
        }
        gfx_width = raster_width;
    }
    return start_raster();
}

void output_select_close(unsigned int prnr)
{
    (void)prnr;
    render_raster();
}

int output_select_putc(unsigned int prnr, uint8_t b)
{
    (void)prnr;
    if (raster == NULL && start_raster() < 0) {
        return -1;
    }
    if (b == OUTPUT_NEWLINE) {
        raster_x = 0;
        raster_y++;
        if (raster_y == raster_height) {
            render_raster();
        } else if (raster_y > raster_height) {
            raster_error = 1;
            return -1;
        }
    } else if (raster_y < raster_height && raster_x < raster_width) {
        raster[raster_y * raster_width + raster_x++] = (char)b;
    } else {
        raster_error = 1;
        return -1;
    }
    return 0;
}

int output_select_getc(unsigned int prnr, uint8_t *b)
{
    (void)prnr;
    (void)b;
    return 0;
}

int output_select_flush(unsigned int prnr)
{
    (void)prnr;
    return 0;
}

int output_select_formfeed(unsigned int prnr)
{
    (void)prnr;
    render_raster();
    return 0;
}

void vice_helper_set_output_options(int write_stdout, int write_gfx)
{
    stdout_enabled = write_stdout;
    gfx_enabled = write_gfx;
}

int vice_helper_output_error(void)
{
    return raster_error;
}

const uint8_t *vice_helper_gfx_pixels(void)
{
    return gfx_pixels;
}

size_t vice_helper_gfx_rows(void)
{
    return gfx_rows;
}

size_t vice_helper_gfx_width(void)
{
    return gfx_width;
}

void vice_helper_free_gfx_pixels(void)
{
    free(gfx_pixels);
    gfx_pixels = NULL;
    gfx_rows = 0;
    gfx_capacity = 0;
    gfx_width = 0;
}

void *lib_malloc(size_t size)
{
    return malloc(size);
}

void *lib_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

void *lib_malloc_pinpoint(size_t size, const char *name, unsigned int line)
{
    (void)name;
    (void)line;
    return malloc(size);
}

void lib_free(void *ptr)
{
    free(ptr);
}

void lib_free_pinpoint(void *ptr, const char *name, unsigned int line)
{
    (void)name;
    (void)line;
    free(ptr);
}

log_t log_open(const char *id)
{
    (void)id;
    return LOG_DEFAULT;
}

int log_warning(log_t log, const char *format, ...)
{
    va_list args;
    int result;

    (void)log;
    va_start(args, format);
    result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}

int log_message(log_t log, const char *format, ...)
{
    va_list args;
    int result;

    (void)log;
    va_start(args, format);
    result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}

int log_error(log_t log, const char *format, ...)
{
    va_list args;
    int result;

    (void)log;
    va_start(args, format);
    result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}

palette_t *palette_create(unsigned int num_entries,
                          const char *entry_names[])
{
    palette_t *palette = malloc(sizeof(*palette));

    (void)entry_names;
    if (palette == NULL) {
        return NULL;
    }
    palette->entries = calloc(num_entries, sizeof(*palette->entries));
    if (palette->entries == NULL) {
        free(palette);
        return NULL;
    }
    palette->num_entries = num_entries;
    return palette;
}

void palette_free(palette_t *palette)
{
    if (palette != NULL) {
        free(palette->entries);
        free(palette);
    }
}

int palette_load(const char *file_name, const char *subpath,
                 palette_t *palette_return)
{
    (void)file_name;
    (void)subpath;
    (void)palette_return;
    return 0;
}

int sysfile_load(const char *name, const char *subpath, uint8_t *dest,
                 int minsize, int maxsize)
{
    const char *rom;
    size_t rom_size;

    (void)name;
    (void)subpath;
    if (strcmp(name, NL10_ROM_NAME) == 0) {
        rom = nl10_standalone_rom;
        rom_size = sizeof(nl10_standalone_rom);
    } else if (strcmp(name, MPS803_ROM_NAME) == 0) {
        rom = mps803_standalone_rom;
        rom_size = sizeof(mps803_standalone_rom);
    } else {
        DEBUG_LOG("No embedded ROM is available for '%s'.\n", name);
        return -1;
    }
    if (rom_size < (size_t)minsize || rom_size > (size_t)maxsize) {
        DEBUG_LOG("The embedded ROM '%s' has an invalid size.\n", name);
        return -1;
    }
    memcpy(dest, rom, rom_size);
    return (int)rom_size;
}

int userport_get_device(void)
{
    return USERPORT_DEVICE_PRINTER;
}
