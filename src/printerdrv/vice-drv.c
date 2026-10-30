/*
 * vice-drv.c - Standalone printer-driver demonstration.
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drv-nl10.h"
#include "drv-mps803.h"
#include "drv-1520.h"
#include "vice-helper.h"
#if defined(__CIRCLE__)
#include <circle/types.h>
#include <fatfs/ff.h>
#else
#include "ff-local.h"
#endif

#define TEST_GLYPHS_PER_LINE 5
#define TEST_GLYPH_COUNT 192
#define PNG_MARGIN 10
#define PNG_MAX_WIDTH 1920
#define PNG_MAX_CONTENT_WIDTH VICE_HELPER_GFX_MAX_WIDTH
#define PNG_MAX_BLOCK_SIZE 65535

static driver_select_t *printer_driver;
static int test_mode;
static int stdout_enabled;
static const char *gfx_path;

static uint32_t png_crc_update(uint32_t crc, const uint8_t *data, size_t size)
{
    size_t i;
    unsigned int bit;

    for (i = 0; i < size; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xedb88320U & (uint32_t)-(int32_t)(crc & 1));
        }
    }
    return crc;
}

static int write_file(FIL *file, const void *data, UINT size)
{
    UINT bytes_written;

    return f_write(file, data, size, &bytes_written) == FR_OK
        && bytes_written == size;
}

static int write_png_chunk(FIL *file, const char *type,
                           const uint8_t *data, uint32_t size)
{
    uint8_t length[4], checksum[4];
    uint32_t crc;

    length[0] = (uint8_t)(size >> 24);
    length[1] = (uint8_t)(size >> 16);
    length[2] = (uint8_t)(size >> 8);
    length[3] = (uint8_t)size;
    crc = png_crc_update(0xffffffffU, (const uint8_t *)type, 4);
    crc = png_crc_update(crc, data, size) ^ 0xffffffffU;
    checksum[0] = (uint8_t)(crc >> 24);
    checksum[1] = (uint8_t)(crc >> 16);
    checksum[2] = (uint8_t)(crc >> 8);
    checksum[3] = (uint8_t)crc;

    return write_file(file, length, sizeof(length))
        && write_file(file, type, 4)
        && (size == 0 || write_file(file, data, size))
        && write_file(file, checksum, sizeof(checksum))
        ? 0 : -1;
}

static int write_gfx_png(void)
{
    FIL file;
    uint8_t header[13], *raw = NULL, *compressed = NULL;
    const uint8_t *gfx_pixels = vice_helper_gfx_pixels();
    size_t gfx_rows = vice_helper_gfx_rows();
    size_t gfx_width = vice_helper_gfx_width();
    size_t left = gfx_width, right = 0, top = gfx_rows, bottom = 0;
    size_t content_width, output_content_width, content_height;
    size_t image_width, image_height;
    size_t raw_size, block_count, compressed_size, source_y, source_x;
    size_t raw_offset = 0, compressed_offset = 0;
    uint32_t adler_a = 1, adler_b = 0;
    uint8_t signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    int found_pixel = 0;
    int result = -1;

    for (source_y = 0; source_y < gfx_rows; source_y++) {
        for (source_x = 0; source_x < gfx_width; source_x++) {
            if (gfx_pixels[source_y * gfx_width + source_x]
                == OUTPUT_PIXEL_BLACK) {
                if (!found_pixel || source_x < left) {
                    left = source_x;
                }
                if (!found_pixel || source_x > right) {
                    right = source_x;
                }
                if (!found_pixel || source_y < top) {
                    top = source_y;
                }
                if (!found_pixel || source_y > bottom) {
                    bottom = source_y;
                }
                found_pixel = 1;
            }
        }
    }

    content_width = found_pixel ? right - left + 1 : 0;
    output_content_width = content_width > PNG_MAX_CONTENT_WIDTH
        ? PNG_MAX_CONTENT_WIDTH : content_width;
    content_height = found_pixel ? bottom - top + 1 : 0;
    image_width = output_content_width + 2 * PNG_MARGIN;
    image_height = content_height + 2 * PNG_MARGIN;
    if (image_height > 0x7fffffffU
        || image_width > PNG_MAX_WIDTH
        || image_width + 1 > SIZE_MAX / image_height) {
        DEBUG_LOG("PNG dimensions exceed the supported size.");
        return -1;
    }
    raw_size = (image_width + 1) * image_height;
    block_count = raw_size / PNG_MAX_BLOCK_SIZE
        + (raw_size % PNG_MAX_BLOCK_SIZE != 0);
    if (raw_size > SIZE_MAX - 6
        || block_count > (SIZE_MAX - raw_size - 6) / 5) {
        DEBUG_LOG("PNG image is too large to encode.");
        return -1;
    }
    compressed_size = 2 + raw_size + block_count * 5 + 4;
    if (compressed_size > UINT32_MAX) {
        DEBUG_LOG("PNG image exceeds the maximum chunk size.");
        return -1;
    }

    raw = malloc(raw_size);
    compressed = malloc(compressed_size);
    if (raw == NULL || compressed == NULL) {
        DEBUG_LOG("Cannot allocate PNG image data.");
        goto cleanup;
    }
    for (source_y = 0; source_y < image_height; source_y++) {
        size_t x;
        raw[raw_offset++] = 0;
        for (x = 0; x < image_width; x++) {
            uint8_t pixel = OUTPUT_PIXEL_WHITE;
            size_t image_x = x;
            size_t image_y = source_y;

            if (found_pixel && image_x >= PNG_MARGIN
                && image_x < PNG_MARGIN + output_content_width
                && image_y >= PNG_MARGIN
                && image_y < PNG_MARGIN + content_height) {
                size_t output_x = image_x - PNG_MARGIN;
                size_t source_start = left
                    + output_x * content_width / output_content_width;
                size_t source_end = left
                    + (output_x + 1) * content_width / output_content_width;
                size_t source_col;
                size_t black_pixels = 0;
                size_t source_count;

                if (source_end == source_start) {
                    source_end++;
                }
                for (source_col = source_start; source_col < source_end;
                     source_col++) {
                    if (gfx_pixels[(top + image_y - PNG_MARGIN) * gfx_width
                                   + source_col] == OUTPUT_PIXEL_BLACK) {
                        black_pixels++;
                    }
                }
                source_count = source_end - source_start;
                raw[raw_offset++] = (uint8_t)(
                    255 - black_pixels * 255 / source_count);
                continue;
            }
            raw[raw_offset++] = pixel == OUTPUT_PIXEL_BLACK ? 0 : 255;
        }
    }

    compressed[compressed_offset++] = 0x78;
    compressed[compressed_offset++] = 0x01;
    raw_offset = 0;
    while (raw_offset < raw_size) {
        size_t block_size = raw_size - raw_offset;
        uint16_t length;
        uint16_t inverse_length;

        if (block_size > PNG_MAX_BLOCK_SIZE) {
            block_size = PNG_MAX_BLOCK_SIZE;
        }
        length = (uint16_t)block_size;
        inverse_length = (uint16_t)~length;
        compressed[compressed_offset++] =
            raw_offset + block_size == raw_size ? 1 : 0;
        compressed[compressed_offset++] = (uint8_t)length;
        compressed[compressed_offset++] = (uint8_t)(length >> 8);
        compressed[compressed_offset++] = (uint8_t)inverse_length;
        compressed[compressed_offset++] = (uint8_t)(inverse_length >> 8);
        memcpy(compressed + compressed_offset, raw + raw_offset, block_size);
        compressed_offset += block_size;
        raw_offset += block_size;
    }
    for (raw_offset = 0; raw_offset < raw_size; raw_offset++) {
        adler_a = (adler_a + raw[raw_offset]) % 65521;
        adler_b = (adler_b + adler_a) % 65521;
    }
    compressed[compressed_offset++] = (uint8_t)(adler_b >> 8);
    compressed[compressed_offset++] = (uint8_t)adler_b;
    compressed[compressed_offset++] = (uint8_t)(adler_a >> 8);
    compressed[compressed_offset++] = (uint8_t)adler_a;

    if (f_open(&file, gfx_path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        DEBUG_LOG("Cannot open PNG output file '%s'.\n", gfx_path);
        goto cleanup;
    }
    header[0] = (uint8_t)(image_width >> 24);
    header[1] = (uint8_t)(image_width >> 16);
    header[2] = (uint8_t)(image_width >> 8);
    header[3] = (uint8_t)image_width;
    header[4] = (uint8_t)(image_height >> 24);
    header[5] = (uint8_t)(image_height >> 16);
    header[6] = (uint8_t)(image_height >> 8);
    header[7] = (uint8_t)image_height;
    header[8] = 8;
    header[9] = 0;
    header[10] = 0;
    header[11] = 0;
    header[12] = 0;
    if (write_file(&file, signature, sizeof(signature))
        && write_png_chunk(&file, "IHDR", header, sizeof(header)) == 0
        && write_png_chunk(&file, "IDAT", compressed,
                           (uint32_t)compressed_offset) == 0
        && write_png_chunk(&file, "IEND", NULL, 0) == 0) {
        result = 0;
    } else {
        DEBUG_LOG("Failed to write PNG output.");
    }
    if (f_close(&file) != FR_OK) {
        DEBUG_LOG("Failed to close PNG output file.");
        result = -1;
    }

cleanup:
    free(raw);
    free(compressed);
    return result;
}

#if defined(__CIRCLE__)
extern const char *prBuffer4vicedrv;
extern size_t bufferlen;
int vicepr_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    uint8_t input[4096];
    size_t bytes_read;
    size_t i;
    int arg;
    const char *driver_name = "nl10";
    int (*init_resources)(void);
    int (*init_driver)(void);
    void (*shutdown_driver)(void);
    int saw_input = 0;
    int last_was_formfeed = 0;
    int status = EXIT_SUCCESS;

    for (arg = 1; arg < argc; arg++) {
        DEBUG_LOG("%s: processing '%s'", __FUNCTION__, argv[arg]);
        if (strcmp(argv[arg], "--help") == 0) {
            DEBUG_LOG("Usage: %s [--drv nl10|mps803|1520] [--test] [--stdout] "
                   "[--gfx [path.png]] < input-bytes\n",
                   argv[0]);
            puts("  --drv    select nl10, mps803, or 1520 (default: nl10)");
            puts("  --test   print CBM glyphs supported by the selected driver");
            puts("  --stdout write the rendered raster to stdout");
            puts("  --gfx   write PNG output to path, or printer.png if omitted");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[arg], "--test") == 0) {
            test_mode = 1;
        } else if (strcmp(argv[arg], "--stdout") == 0) {
            stdout_enabled = 1;
        } else if (strcmp(argv[arg], "--drv") == 0 && arg + 1 < argc) {
            driver_name = argv[++arg];
            if (strcmp(driver_name, "nl10") != 0
                && strcmp(driver_name, "mps803") != 0
                && strcmp(driver_name, "1520") != 0) {
                DEBUG_LOG("Unsupported printer driver '%s'; "
                        "choose nl10, mps803, or 1520.\n", driver_name);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[arg], "--gfx") == 0) {
            gfx_path = "printer.png";
            if (arg + 1 < argc && argv[arg + 1][0] != '-') {
                gfx_path = argv[++arg];
            }
        } else {
            DEBUG_LOG("Usage: %s [--drv nl10|mps803|1520] [--test] "
                    "[--stdout] [--gfx [path.png]] < input-bytes\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (strcmp(driver_name, "nl10") == 0) {
        init_resources = drv_nl10_init_resources;
        init_driver = drv_nl10_init;
        shutdown_driver = drv_nl10_shutdown;
        if (gfx_path != NULL) {
            drv_nl10_set_standalone_gfx_width(PNG_MAX_CONTENT_WIDTH);
        }
    } else if (strcmp(driver_name, "mps803") == 0) {
        init_resources = drv_mps803_init_resources;
        init_driver = drv_mps803_init;
        shutdown_driver = drv_mps803_shutdown;
    } else {
        init_resources = drv_1520_init_resources;
        init_driver = drv_1520_init;
        shutdown_driver = drv_1520_shutdown;
    }
    vice_helper_select_driver(driver_name);
    vice_helper_set_output_options(stdout_enabled, gfx_path != NULL);

    if (init_resources() < 0
        || (printer_driver = vice_helper_driver()) == NULL) {
        DEBUG_LOG("Cannot register the '%s' printer driver.\n",
                driver_name);
        return EXIT_FAILURE;
    }
    if (init_driver() < 0
        || printer_driver->drv_open(0, DRIVER_FIRST_OPEN) < 0
        || printer_driver->drv_flush(0, 0) < 0
        || printer_driver->drv_open(0, 0) < 0) {
        DEBUG_LOG("The '%s' driver failed to initialize.\n",
                driver_name);
        status = EXIT_FAILURE;
    } else if (test_mode) {
        unsigned int character, printed = 0;

        for (character = 0; character <= 255; character++) {
            /* The NL10 protocol reserves these ranges for control codes. */
            if (character < 0x20 || (character >= 0x80 && character < 0xa0)) {
                continue;
            }
            if (printer_driver->drv_putc(0, 0, (uint8_t)character) < 0) {
                DEBUG_LOG("The '%s' driver failed during the "
                        "character test.\n", driver_name);
                status = EXIT_FAILURE;
                break;
            }
            printed++;
            if (printed % TEST_GLYPHS_PER_LINE == 0
                && printed < TEST_GLYPH_COUNT
                && printer_driver->drv_putc(0, 0, '\r') < 0) {
                DEBUG_LOG("The '%s' driver failed to advance the "
                        "test line.\n", driver_name);
                status = EXIT_FAILURE;
                break;
            }
        }
        if (printer_driver->drv_formfeed(0) < 0) {
            DEBUG_LOG("The '%s' driver failed to finish the "
                    "character test.\n", driver_name);
            status = EXIT_FAILURE;
        }
        DEBUG_LOG("Printer control codes 0x00-0x1f and 0x80-0x9f are omitted "
                  "from the character test.");
        saw_input = 1;
    } else {
#if !defined (__CIRCLE__)        
        while ((bytes_read = fread(input, 1, sizeof(input), stdin)) > 0) {
            for (i = 0; i < bytes_read; i++) {
                if (printer_driver->drv_putc(0, 0, input[i]) < 0) {
                    DEBUG_LOG("The '%s' driver failed to process "
                            "input.\n", driver_name);
                    status = EXIT_FAILURE;
                    break;
                }
                saw_input = 1;
                last_was_formfeed = input[i] == '\f';
            }
            if (status != EXIT_SUCCESS) {
                break;
            }
        }
        if (ferror(stdin)) {
            DEBUG_LOG("Failed to read printer data from stdin.");
            status = EXIT_FAILURE;
        } else 
#else
        for (i = 0; i < bufferlen; i++) {
            if (printer_driver->drv_putc(0, 0, prBuffer4vicedrv[i]) < 0) 
            {
                DEBUG_LOG("The '%s' driver failed to process input.", driver_name);
                    status = EXIT_FAILURE;
                    break;
            }
            saw_input = 1;
            last_was_formfeed = prBuffer4vicedrv[i] == '\f';
        }
#endif            
        if (saw_input && !last_was_formfeed
                   && printer_driver->drv_formfeed(0) < 0) {
            DEBUG_LOG("The '%s' driver failed to finish the final page.", driver_name);
            status = EXIT_FAILURE;
        }
    }

    shutdown_driver();
    if (vice_helper_output_error()) {
        DEBUG_LOG("Failed to capture or write printer output.");
        status = EXIT_FAILURE;
    }
    if (gfx_path != NULL && write_gfx_png() < 0) {
        status = EXIT_FAILURE;
    }
    vice_helper_free_gfx_pixels();
    return status;
}
