#include "vice-helper.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__CIRCLE__)
#include <limits.h>
#include <fatfs/ff.h>
#endif

#include "mps803-rom.h"
#include "mps801-rom.h"
#include "cbm1526-rom.h"
#include "nl10-rom.h"

#define PNG_MAX_CONTENT_WIDTH VICE_HELPER_GFX_MAX_WIDTH

struct vice_helper_file_s {
#if defined(__CIRCLE__)
    FIL stream;
    FILE *console_input;
    int type;
    int owned;
    int append;
    int error;
#else
    FILE *stream;
    int owned;
#endif
};

#if defined(__CIRCLE__)
enum {
    VICE_HELPER_FILE,
    VICE_HELPER_STDIN,
    VICE_HELPER_STDOUT,
    VICE_HELPER_STDERR
};

static vice_helper_file_t standard_input;
static vice_helper_file_t standard_output;
static vice_helper_file_t standard_error;

static size_t circle_console_write(const void *buffer, size_t length,
                                  vice_helper_file_t *file)
{
    const char *data = buffer;
    char text[2048];
    size_t written = 0;

    while (written < length) {
        size_t chunk_length = length - written;
        const char *end;

        if (chunk_length >= sizeof(text)) {
            chunk_length = sizeof(text) - 1;
        }
        end = memchr(data + written, '\0', chunk_length);
        if (end != NULL) {
            chunk_length = (size_t)(end - (data + written));
            file->error = 1;
            if (chunk_length == 0) {
                break;
            }
        }
        memcpy(text, data + written, chunk_length);
        text[chunk_length] = '\0';
        klog("%s", text);
        written += chunk_length;
        if (end != NULL) {
            break;
        }
    }

    return written;
}
#endif

#if !defined(__CIRCLE__)
static vice_helper_file_t standard_input = { NULL, 0 };
static vice_helper_file_t standard_output = { NULL, 0 };
static vice_helper_file_t standard_error = { NULL, 0 };
#endif

#if !defined(__CIRCLE__)
vice_helper_file_t *vice_helper_file_open(const char *path, const char *mode)
{
    vice_helper_file_t *file = malloc(sizeof(*file));

    if (file == NULL) {
        return NULL;
    }
    file->stream = fopen(path, mode);
    if (file->stream == NULL) {
        free(file);
        return NULL;
    }
    file->owned = 1;
    return file;
}

vice_helper_file_t *vice_helper_stdin(void)
{
    standard_input.stream = stdin;
    return &standard_input;
}

vice_helper_file_t *vice_helper_stdout(void)
{
    standard_output.stream = stdout;
    return &standard_output;
}

vice_helper_file_t *vice_helper_stderr(void)
{
    standard_error.stream = stderr;
    return &standard_error;
}

size_t vice_helper_file_read(void *buffer, size_t size, size_t count,
                             vice_helper_file_t *file)
{
    return fread(buffer, size, count, file->stream);
}

size_t vice_helper_file_write(const void *buffer, size_t size, size_t count,
                              vice_helper_file_t *file)
{
    return fwrite(buffer, size, count, file->stream);
}

int vice_helper_file_close(vice_helper_file_t *file)
{
    int result = fclose(file->stream);

    if (file->owned) {
        free(file);
    }
    return result;
}

int vice_helper_file_putc(int character, vice_helper_file_t *file)
{
    return fputc(character, file->stream);
}

int vice_helper_file_puts(const char *string, vice_helper_file_t *file)
{
    return fputs(string, file->stream);
}

int vice_helper_file_vprintf(vice_helper_file_t *file, const char *format,
                             va_list args)
{
    return vfprintf(file->stream, format, args);
}

int vice_helper_file_printf(vice_helper_file_t *file, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = vice_helper_file_vprintf(file, format, args);
    va_end(args);
    return result;
}

int vice_helper_puts(const char *string)
{
    vice_helper_file_t *output = vice_helper_stdout();

    if (vice_helper_file_puts(string, output) == EOF) {
        return EOF;
    }
    return vice_helper_file_putc('\n', output) == EOF ? EOF : 0;
}

int vice_helper_file_flush(vice_helper_file_t *file)
{
    return fflush(file->stream);
}

long vice_helper_file_tell(vice_helper_file_t *file)
{
    return ftell(file->stream);
}

int vice_helper_file_error(vice_helper_file_t *file)
{
    return ferror(file->stream);
}

void vice_helper_debug_log(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vice_helper_file_vprintf(vice_helper_stdout(), format, args);
    va_end(args);
}
#else
vice_helper_file_t *vice_helper_file_open(const char *path, const char *mode)
{
    vice_helper_file_t *file;
    const char *option;
    BYTE flags;
    BYTE create_mode;
    int plus = 0;
    int exclusive = 0;
    int binary = 0;
    FRESULT result;

    if (path == NULL || mode == NULL || mode[0] == '\0') {
        return NULL;
    }

    switch (mode[0]) {
    case 'r':
        flags = FA_READ;
        create_mode = FA_OPEN_EXISTING;
        break;
    case 'w':
        flags = FA_WRITE;
        create_mode = FA_CREATE_ALWAYS;
        break;
    case 'a':
        flags = FA_WRITE;
        create_mode = FA_OPEN_APPEND;
        break;
    default:
        return NULL;
    }

    for (option = mode + 1; *option != '\0'; option++) {
        switch (*option) {
        case '+':
            if (plus) {
                return NULL;
            }
            plus = 1;
            flags |= FA_WRITE | FA_READ;
            break;
        case 'b':
            if (binary) {
                return NULL;
            }
            binary = 1;
            break;
        case 'x':
            if (exclusive || mode[0] != 'w') {
                return NULL;
            }
            exclusive = 1;
            create_mode = FA_CREATE_NEW;
            break;
        default:
            return NULL;
        }
    }

    file = calloc(1, sizeof(*file));
    if (file == NULL) {
        return NULL;
    }

    result = f_open(&file->stream, path, flags | create_mode);
    if (result != FR_OK) {
        free(file);
        return NULL;
    }

    file->type = VICE_HELPER_FILE;
    file->owned = 1;
    file->append = mode[0] == 'a';
    if (file->append && plus && f_lseek(&file->stream, 0) != FR_OK) {
        f_close(&file->stream);
        free(file);
        return NULL;
    }
    return file;
}

vice_helper_file_t *vice_helper_stdin(void)
{
    standard_input.type = VICE_HELPER_STDIN;
    standard_input.console_input = stdin;
    return &standard_input;
}

vice_helper_file_t *vice_helper_stdout(void)
{
    standard_output.type = VICE_HELPER_STDOUT;
    return &standard_output;
}

vice_helper_file_t *vice_helper_stderr(void)
{
    standard_error.type = VICE_HELPER_STDERR;
    return &standard_error;
}

size_t vice_helper_file_read(void *buffer, size_t size, size_t count,
                             vice_helper_file_t *file)
{
    size_t total;
    size_t bytes_read = 0;

    if (file == NULL || size == 0 || count == 0) {
        return 0;
    }
    if (count > (size_t)-1 / size) {
        file->error = 1;
        return 0;
    }
    if (buffer == NULL) {
        file->error = 1;
        return 0;
    }
    if (file->type == VICE_HELPER_STDIN) {
        size_t items_read = fread(buffer, size, count, file->console_input);

        if (ferror(file->console_input)) {
            file->error = 1;
        }
        return items_read;
    }
    if (file->type != VICE_HELPER_FILE || !file->owned) {
        file->error = 1;
        return 0;
    }

    total = size * count;
    while (bytes_read < total) {
        size_t remaining = total - bytes_read;
        UINT request = remaining > (size_t)((UINT)-1)
            ? (UINT)-1 : (UINT)remaining;
        UINT transferred = 0;
        FRESULT result = f_read(&file->stream, (char *)buffer + bytes_read,
                                request, &transferred);

        bytes_read += transferred;
        if (result != FR_OK) {
            file->error = 1;
            break;
        }
        if (transferred < request) {
            break;
        }
    }
    return bytes_read / size;
}

size_t vice_helper_file_write(const void *buffer, size_t size, size_t count,
                              vice_helper_file_t *file)
{
    size_t total;
    size_t bytes_written = 0;

    if (file == NULL || size == 0 || count == 0) {
        return 0;
    }
    if (count > (size_t)-1 / size) {
        file->error = 1;
        return 0;
    }
    if (buffer == NULL) {
        file->error = 1;
        return 0;
    }
    total = size * count;
    if (file->type == VICE_HELPER_STDOUT || file->type == VICE_HELPER_STDERR) {
        return circle_console_write(buffer, total, file) / size;
    }
    if (file->type != VICE_HELPER_FILE || !file->owned || buffer == NULL) {
        file->error = 1;
        return 0;
    }

    if (file->append && f_lseek(&file->stream, f_size(&file->stream)) != FR_OK) {
        file->error = 1;
        return 0;
    }
    while (bytes_written < total) {
        size_t remaining = total - bytes_written;
        UINT request = remaining > (size_t)((UINT)-1)
            ? (UINT)-1 : (UINT)remaining;
        UINT transferred = 0;
        FRESULT result = f_write(&file->stream,
                                 (const char *)buffer + bytes_written,
                                 request, &transferred);

        bytes_written += transferred;
        if (result != FR_OK) {
            file->error = 1;
            break;
        }
        if (transferred < request) {
            file->error = 1;
            break;
        }
    }
    return bytes_written / size;
}

int vice_helper_file_close(vice_helper_file_t *file)
{
    FRESULT result;

    if (file == NULL) {
        return -1;
    }
    if (file->type != VICE_HELPER_FILE || !file->owned) {
        return 0;
    }

    result = f_close(&file->stream);
    free(file);
    return result == FR_OK ? 0 : -1;
}

int vice_helper_file_putc(int character, vice_helper_file_t *file)
{
    unsigned char byte = (unsigned char)character;

    return vice_helper_file_write(&byte, 1, 1, file) == 1
        ? byte : EOF;
}

int vice_helper_file_puts(const char *string, vice_helper_file_t *file)
{
    size_t length;

    if (string == NULL) {
        return EOF;
    }
    length = strlen(string);
    return vice_helper_file_write(string, 1, length, file) == length
        ? 0 : EOF;
}

int vice_helper_file_vprintf(vice_helper_file_t *file, const char *format,
                             va_list args)
{
    va_list copy;
    char *buffer;
    int length;
    int result;

    if (file == NULL || format == NULL) {
        return -1;
    }
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        file->error = 1;
        return -1;
    }

    buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        file->error = 1;
        return -1;
    }
    va_copy(copy, args);
    result = vsnprintf(buffer, (size_t)length + 1, format, copy);
    va_end(copy);
    if (result != length) {
        free(buffer);
        file->error = 1;
        return -1;
    }

    result = vice_helper_file_write(buffer, 1, (size_t)length, file) == (size_t)length
        ? length : -1;
    free(buffer);
    return result;
}

int vice_helper_file_printf(vice_helper_file_t *file, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = vice_helper_file_vprintf(file, format, args);
    va_end(args);
    return result;
}

int vice_helper_puts(const char *string)
{
    vice_helper_file_t *output = vice_helper_stdout();

    if (vice_helper_file_puts(string, output) == EOF) {
        return EOF;
    }
    return vice_helper_file_putc('\n', output) == EOF ? EOF : 0;
}

int vice_helper_file_flush(vice_helper_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    if (file->type == VICE_HELPER_STDOUT || file->type == VICE_HELPER_STDERR) {
        return 0;
    }
    if (file->type != VICE_HELPER_FILE || !file->owned
        || f_sync(&file->stream) != FR_OK) {
        file->error = 1;
        return -1;
    }
    return 0;
}

long vice_helper_file_tell(vice_helper_file_t *file)
{
    FSIZE_t position;

    if (file == NULL || file->type != VICE_HELPER_FILE || !file->owned) {
        return -1;
    }
    position = f_tell(&file->stream);
    return position <= (FSIZE_t)LONG_MAX ? (long)position : -1;
}

int vice_helper_file_error(vice_helper_file_t *file)
{
    if (file == NULL) {
        return 1;
    }
    return file->error
        || (file->type == VICE_HELPER_STDIN && ferror(file->console_input))
        || (file->type == VICE_HELPER_FILE && f_error(&file->stream));
}
#endif

int vice_helper_snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

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
static unsigned int gfx_dpi_x;
static unsigned int gfx_dpi_y;

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
                DEBUG_LOG("Cannot store raster for graphics output.");
                raster_error = 1;
                break;
            }
            if (stdout_enabled
                && (vice_helper_file_write(raster + y * raster_width + left,
                                           1, row_width,
                                           vice_helper_stdout()) != row_width
                    || vice_helper_file_putc('\n',
                                             vice_helper_stdout()) == EOF)) {
                raster_error = 1;
                break;
            }
        }
        if (stdout_enabled && vice_helper_file_flush(vice_helper_stdout()) != 0) {
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
            DEBUG_LOG("Printer output dimensions changed during graphics "
                      "capture.");
            raster_error = 1;
            return -1;
        }
        if (gfx_rows > 0
            && (gfx_dpi_x != output_parameter->dpi_x
                || gfx_dpi_y != output_parameter->dpi_y)) {
            DEBUG_LOG("Printer output resolution changed during graphics "
                      "capture.");
            raster_error = 1;
            return -1;
        }
        gfx_width = raster_width;
        gfx_dpi_x = output_parameter->dpi_x;
        gfx_dpi_y = output_parameter->dpi_y;
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

unsigned int vice_helper_gfx_dpi_x(void)
{
    return gfx_dpi_x;
}

unsigned int vice_helper_gfx_dpi_y(void)
{
    return gfx_dpi_y;
}

void vice_helper_free_gfx_pixels(void)
{
    free(gfx_pixels);
    gfx_pixels = NULL;
    gfx_rows = 0;
    gfx_capacity = 0;
    gfx_width = 0;
    gfx_dpi_x = 0;
    gfx_dpi_y = 0;
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
    result = vice_helper_file_vprintf(vice_helper_stderr(), format, args);
    va_end(args);
    return result;
}

int log_message(log_t log, const char *format, ...)
{
    va_list args;
    int result;

    (void)log;
    va_start(args, format);
    result = vice_helper_file_vprintf(vice_helper_stderr(), format, args);
    va_end(args);
    return result;
}

int log_error(log_t log, const char *format, ...)
{
    va_list args;
    int result;

    (void)log;
    va_start(args, format);
    result = vice_helper_file_vprintf(vice_helper_stderr(), format, args);
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
    } else if (strcmp(name, MPS801_ROM_NAME) == 0) {
        rom = (const char *)mps801_standalone_rom;
        rom_size = sizeof(mps801_standalone_rom);
    } else if (strcmp(name, MPS802_ROM_NAME) == 0) {
        rom = (const char *)cbm1526_standalone_rom;
        rom_size = sizeof(cbm1526_standalone_rom);
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
