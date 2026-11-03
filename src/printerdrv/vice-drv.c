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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "drv-nl10.h"
#include "drv-mps803.h"
#include "drv-1520.h"
#include "vice-helper.h"
#include "../miniz.h"

#define TEST_GLYPHS_PER_LINE 5
#define TEST_GLYPH_COUNT 192
#define PNG_MARGIN 10
#define PNG_MAX_WIDTH 4096
#define PDF_MARGIN 10

static driver_select_t *printer_driver;
static int test_mode;
static int stdout_enabled;
static const char *gfx_path;
static const char *pdf_path;
#if defined(__CIRCLE__)
static int vicepr_session_active;
static char *vicepr_session_driver;
static char *vicepr_session_gfx_path;
static char *vicepr_session_pdf_path;
static void (*vicepr_session_shutdown_driver)(void);
static int vicepr_session_had_input;
static int vicepr_session_last_was_formfeed;
#endif

#if defined(__CIRCLE__)
static char *duplicate_string(const char *string)
{
    size_t length;
    char *copy;

    if (string == NULL) {
        return NULL;
    }
    length = strlen(string) + 1;
    copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, string, length);
    }
    return copy;
}

static int same_optional_string(const char *left, const char *right)
{
    return (left == NULL && right == NULL)
        || (left != NULL && right != NULL && strcmp(left, right) == 0);
}

static void clear_vicepr_session(void)
{
    free(vicepr_session_driver);
    free(vicepr_session_gfx_path);
    free(vicepr_session_pdf_path);
    vicepr_session_driver = NULL;
    vicepr_session_gfx_path = NULL;
    vicepr_session_pdf_path = NULL;
    vicepr_session_shutdown_driver = NULL;
    vicepr_session_active = 0;
    vicepr_session_had_input = 0;
    vicepr_session_last_was_formfeed = 0;
    printer_driver = NULL;
    gfx_path = NULL;
    pdf_path = NULL;
    vice_helper_free_gfx_pixels();
}
#endif

static char *numbered_gfx_path(size_t page_number)
{
    const char *extension = strrchr(gfx_path, '.');
    const char *separator = strrchr(gfx_path, '/');
    size_t path_length = strlen(gfx_path);
    size_t prefix_length;
    size_t allocation_size;
    char *path;
    int path_length_written;

    if (extension == NULL || (separator != NULL && extension < separator)) {
        extension = gfx_path + path_length;
    }
    prefix_length = (size_t)(extension - gfx_path);
    if (path_length > SIZE_MAX - 32) {
        return NULL;
    }
    allocation_size = path_length + 32;
    path = malloc(allocation_size);
    if (path == NULL) {
        return NULL;
    }
    memcpy(path, gfx_path, prefix_length);
    path_length_written = vice_helper_snprintf(
        path + prefix_length, allocation_size - prefix_length,
        "-%lu%s", (unsigned long)page_number, extension);
    if (path_length_written < 0
        || (size_t)path_length_written >= allocation_size - prefix_length) {
        free(path);
        return NULL;
    }
    return path;
}

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

static int write_png_chunk(vice_helper_file_t *file, const char *type,
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

    return vice_helper_file_write(length, 1, sizeof(length), file)
               == sizeof(length)
        && vice_helper_file_write(type, 1, 4, file) == 4
        && (size == 0 || vice_helper_file_write(data, 1, size, file) == size)
        && vice_helper_file_write(checksum, 1, sizeof(checksum), file)
               == sizeof(checksum)
        ? 0 : -1;
}

typedef struct compressed_output_s {
    vice_helper_file_t *file;
    size_t length;
    int png;
} compressed_output_t;

static mz_bool write_compressed_output(const void *data, int length,
                                       void *user_data)
{
    compressed_output_t *output = user_data;
    size_t bytes = (size_t)length;
    int result;

    if (length < 0 || bytes > SIZE_MAX - output->length) {
        return MZ_FALSE;
    }
    if (output->png) {
        result = write_png_chunk(output->file, "IDAT", data,
                                 (uint32_t)bytes);
        if (result < 0) {
            return MZ_FALSE;
        }
    } else if (vice_helper_file_write(data, 1, bytes, output->file) != bytes) {
        return MZ_FALSE;
    }
    output->length += bytes;
    return MZ_TRUE;
}

static tdefl_compressor *start_image_compression(compressed_output_t *output)
{
    tdefl_compressor *compressor = tdefl_compressor_alloc();
    int flags = tdefl_create_comp_flags_from_zip_params(
        MZ_DEFAULT_COMPRESSION, MZ_DEFAULT_WINDOW_BITS, MZ_DEFAULT_STRATEGY)
        | TDEFL_WRITE_ZLIB_HEADER | TDEFL_COMPUTE_ADLER32;

    if (compressor == NULL) {
        DEBUG_LOG("Cannot allocate image compressor.");
        return NULL;
    }
    if (tdefl_init(compressor, write_compressed_output, output, flags)
        != TDEFL_STATUS_OKAY) {
        DEBUG_LOG("Cannot initialize image compressor.");
        tdefl_compressor_free(compressor);
        return NULL;
    }
    return compressor;
}

static int finish_image_compression(tdefl_compressor *compressor)
{
    tdefl_status status = tdefl_compress_buffer(
        compressor, NULL, 0, TDEFL_FINISH);

    if (status != TDEFL_STATUS_DONE) {
        DEBUG_LOG("Image compression failed.");
        return -1;
    }
    return 0;
}

static int write_gfx_png_page(size_t first_row, size_t last_row,
                              const char *output_path)
{
    vice_helper_file_t *file = NULL;
    tdefl_compressor *compressor = NULL;
    uint8_t header[13], *row_data = NULL;
    const uint8_t *gfx_pixels = vice_helper_gfx_pixels();
    size_t gfx_width = vice_helper_gfx_width();
    size_t content_width = gfx_width;
    size_t content_height = vice_helper_gfx_page_height();
    size_t image_width, image_height;
    size_t row_size, source_y;
    compressed_output_t compressed_output;
    uint8_t signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    int result = -1;

    image_width = content_width + 2 * PNG_MARGIN;
    image_height = content_height + 2 * PNG_MARGIN;
    if (image_height > 0x7fffffffU
        || image_width > PNG_MAX_WIDTH
        || image_width + 1 > SIZE_MAX / image_height) {
        DEBUG_LOG("PNG dimensions exceed the supported size.");
        return -1;
    }
    if (image_width == SIZE_MAX) {
        DEBUG_LOG("PNG row is too large to encode.");
        return -1;
    }
    row_size = image_width + 1;
    row_data = malloc(row_size);
    if (row_data == NULL) {
        DEBUG_LOG("Cannot allocate PNG scanline.");
        goto cleanup;
    }
    file = vice_helper_file_open(output_path, "wb");
    if (file == NULL) {
        DEBUG_LOG("Cannot open PNG output file '%s'.\n", output_path);
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
    compressed_output.file = file;
    compressed_output.length = 0;
    compressed_output.png = 1;
    if (vice_helper_file_write(signature, 1, sizeof(signature), file)
            == sizeof(signature)
        && write_png_chunk(file, "IHDR", header, sizeof(header)) == 0) {
        compressor = start_image_compression(&compressed_output);
        if (compressor == NULL) {
            goto cleanup;
        }
        for (source_y = 0; source_y < image_height; source_y++) {
            size_t x;
            tdefl_status status;

            row_data[0] = 0;

            for (x = 0; x < image_width; x++) {
                size_t image_y = source_y;
                uint8_t pixel = OUTPUT_PIXEL_WHITE;

                if (x >= PNG_MARGIN
                    && x < PNG_MARGIN + content_width
                    && image_y >= PNG_MARGIN
                    && image_y < PNG_MARGIN + content_height) {
                    size_t source_row = first_row + image_y - PNG_MARGIN;

                    if (source_row < last_row) {
                        pixel = gfx_pixels[source_row * gfx_width
                                           + x - PNG_MARGIN];
                    }
                }
                row_data[x + 1] = pixel == OUTPUT_PIXEL_BLACK ? 0 : 255;
            }
            status = tdefl_compress_buffer(
                compressor, row_data, row_size, TDEFL_NO_FLUSH);
            if (status < TDEFL_STATUS_OKAY) {
                DEBUG_LOG("Failed to compress PNG scanline.");
                goto cleanup;
            }
        }
        if (finish_image_compression(compressor) == 0
            && write_png_chunk(file, "IEND", NULL, 0) == 0) {
            result = 0;
        } else {
            DEBUG_LOG("Failed to finish PNG compression or output.");
        }
    } else {
        DEBUG_LOG("Failed to write PNG header.");
    }

cleanup:
    if (compressor != NULL) {
        tdefl_compressor_free(compressor);
    }
    if (file != NULL && vice_helper_file_close(file) != 0) {
        DEBUG_LOG("Failed to close PNG output file.");
        result = -1;
    }
    free(row_data);
    return result;
}

static int write_gfx_png(void)
{
    size_t gfx_rows = vice_helper_gfx_rows();
    size_t page_height = vice_helper_gfx_page_height();
    size_t page_count = page_height == 0
        ? 1 : vice_helper_gfx_page_count();
    size_t page;

    if (page_count == 0) {
        return 0;
    }
    for (page = 0; page < page_count; page++) {
        size_t first_row = page_height == 0
            ? 0 : vice_helper_gfx_page_start(page);
        size_t last_row = page_height == 0
            ? gfx_rows : vice_helper_gfx_page_end(page);
        char *output_path = NULL;
        int result;

        if (page_height != 0) {
            output_path = numbered_gfx_path(page + 1);
            if (output_path == NULL) {
                DEBUG_LOG("Cannot allocate PNG page output path.");
                return -1;
            }
        }

        result = write_gfx_png_page(
            first_row, last_row,
            output_path != NULL ? output_path : gfx_path);
        free(output_path);
        if (result < 0) {
            return -1;
        }
    }
    return 0;
}

typedef struct pdf_gfx_page_s {
    size_t first_row, last_row;
    size_t content_width, content_height, image_width, image_height;
    double page_width, page_height;
    char content[160];
    int content_length;
} pdf_gfx_page_t;

static int write_gfx_pdf_multipage(void)
{
    vice_helper_file_t *file = NULL;
    const uint8_t *gfx_pixels = vice_helper_gfx_pixels();
    size_t gfx_width = vice_helper_gfx_width();
    size_t page_height = vice_helper_gfx_page_height();
    size_t gfx_rows = vice_helper_gfx_rows();
    unsigned int dpi_x = vice_helper_gfx_dpi_x();
    unsigned int dpi_y = vice_helper_gfx_dpi_y();
    size_t page_count = page_height == 0
        ? 1 : vice_helper_gfx_page_count();
    size_t object_count, page_index, object_index;
    pdf_gfx_page_t *pages = NULL;
    long *offsets = NULL;
    long xref_offset;
    uint8_t *row_data = NULL;
    int result = -1;

    if (page_count == 0) {
        return 0;
    }
    if (dpi_x == 0 || dpi_y == 0) {
        DEBUG_LOG("Invalid printer page dimensions for PDF output.");
        return -1;
    }
    if (page_count > (SIZE_MAX - 2) / 4) {
        DEBUG_LOG("PDF contains too many pages.");
        return -1;
    }
    object_count = 2 + page_count * 4;
    if (object_count > SIZE_MAX / sizeof(*offsets) - 1
        || page_count > SIZE_MAX / sizeof(*pages)) {
        DEBUG_LOG("PDF contains too many pages.");
        return -1;
    }
    pages = calloc(page_count, sizeof(*pages));
    offsets = calloc(object_count + 1, sizeof(*offsets));
    if (pages == NULL || offsets == NULL) {
        DEBUG_LOG("Cannot allocate PDF page data.");
        goto cleanup;
    }
    if (gfx_width > SIZE_MAX - 2 * PDF_MARGIN) {
        DEBUG_LOG("PDF scanline is too wide.");
        goto cleanup;
    }
    row_data = malloc(gfx_width + 2 * PDF_MARGIN);
    if (row_data == NULL) {
        DEBUG_LOG("Cannot allocate PDF scanline.");
        goto cleanup;
    }

    for (page_index = 0; page_index < page_count; page_index++) {
        pdf_gfx_page_t *page = &pages[page_index];
        page->first_row = page_height == 0
            ? 0 : vice_helper_gfx_page_start(page_index);
        page->last_row = page_height == 0
            ? gfx_rows : vice_helper_gfx_page_end(page_index);
        page->content_width = gfx_width;
        page->content_height = page_height == 0 ? gfx_rows : page_height;

        if (page->content_width > SIZE_MAX - 2 * PDF_MARGIN
            || page->content_height > SIZE_MAX - 2 * PDF_MARGIN) {
            DEBUG_LOG("PDF image is too large to encode.");
            goto cleanup;
        }
        page->image_width = page->content_width + 2 * PDF_MARGIN;
        page->image_height = page->content_height + 2 * PDF_MARGIN;
        page->page_width = page->image_width * 72.0 / dpi_x;
        page->page_height = page->image_height * 72.0 / dpi_y;
        page->content_length = vice_helper_snprintf(
            page->content, sizeof(page->content),
            "q %.3f 0 0 %.3f 0 0 cm /Im0 Do Q\n",
            page->page_width, page->page_height);
        if (page->content_length < 0
            || (size_t)page->content_length >= sizeof(page->content)) {
            DEBUG_LOG("Cannot format PDF page content.");
            goto cleanup;
        }
    }

    file = vice_helper_file_open(pdf_path, "wb");
    if (file == NULL) {
        DEBUG_LOG("Cannot open PDF output file '%s'.\n", pdf_path);
        goto cleanup;
    }
    if (vice_helper_file_puts("%PDF-1.4\n", file) == VICE_HELPER_EOF) {
        goto write_error;
    }
    offsets[1] = vice_helper_file_tell(file);
    if (offsets[1] < 0
        || vice_helper_file_puts(
            "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
            file) == VICE_HELPER_EOF) {
        goto write_error;
    }
    offsets[2] = vice_helper_file_tell(file);
    if (offsets[2] < 0
        || vice_helper_file_printf(
            file, "2 0 obj\n<< /Type /Pages /Kids [") < 0) {
        goto write_error;
    }
    for (page_index = 0; page_index < page_count; page_index++) {
        if (vice_helper_file_printf(
                file, "%lu 0 R ",
                (unsigned long)(3 + page_index * 4)) < 0) {
            goto write_error;
        }
    }
    if (vice_helper_file_printf(
            file, "] /Count %lu >>\nendobj\n",
            (unsigned long)page_count) < 0) {
        goto write_error;
    }

    for (page_index = 0; page_index < page_count; page_index++) {
        pdf_gfx_page_t *page = &pages[page_index];
        size_t page_object = 3 + page_index * 4;
        size_t image_object = page_object + 1;
        size_t content_object = page_object + 2;
        size_t length_object = page_object + 3;
        compressed_output_t compressed_output;
        tdefl_compressor *compressor;
        size_t image_y;

        offsets[page_object] = vice_helper_file_tell(file);
        if (offsets[page_object] < 0
            || vice_helper_file_printf(
                file,
                "%lu 0 obj\n<< /Type /Page /Parent 2 0 R "
                "/MediaBox [0 0 %.3f %.3f] "
                "/Resources << /XObject << /Im0 %lu 0 R >> >> "
                "/Contents %lu 0 R >>\nendobj\n",
                (unsigned long)page_object, page->page_width,
                page->page_height, (unsigned long)image_object,
                (unsigned long)content_object) < 0) {
            goto write_error;
        }
        offsets[image_object] = vice_helper_file_tell(file);
        if (offsets[image_object] < 0
            || vice_helper_file_printf(
                file,
                "%lu 0 obj\n<< /Type /XObject /Subtype /Image "
                "/Width %lu /Height %lu /ColorSpace /DeviceGray "
                "/BitsPerComponent 8 /Filter /FlateDecode "
                "/Length %lu 0 R >>\nstream\n",
                (unsigned long)image_object,
                (unsigned long)page->image_width,
                (unsigned long)page->image_height,
                (unsigned long)length_object) < 0) {
            goto write_error;
        }

        compressed_output.file = file;
        compressed_output.length = 0;
        compressed_output.png = 0;
        compressor = start_image_compression(&compressed_output);
        if (compressor == NULL) {
            goto cleanup;
        }
        for (image_y = 0; image_y < page->image_height; image_y++) {
            size_t image_x;

            for (image_x = 0; image_x < page->image_width; image_x++) {
                uint8_t pixel = OUTPUT_PIXEL_WHITE;

                if (image_x >= PDF_MARGIN
                    && image_x < PDF_MARGIN + page->content_width
                    && image_y >= PDF_MARGIN
                    && image_y < PDF_MARGIN + page->content_height) {
                    size_t source_row =
                        page->first_row + image_y - PDF_MARGIN;

                    if (source_row < page->last_row) {
                        pixel = gfx_pixels[source_row * gfx_width
                                           + image_x - PDF_MARGIN];
                    }
                }
                row_data[image_x] =
                    pixel == OUTPUT_PIXEL_BLACK ? 0 : 255;
            }
            if (tdefl_compress_buffer(
                    compressor, row_data, page->image_width,
                    TDEFL_NO_FLUSH) < TDEFL_STATUS_OKAY) {
                tdefl_compressor_free(compressor);
                goto write_error;
            }
        }
        if (finish_image_compression(compressor) < 0) {
            tdefl_compressor_free(compressor);
            goto write_error;
        }
        tdefl_compressor_free(compressor);
        if (vice_helper_file_puts("\nendstream\nendobj\n", file)
            == VICE_HELPER_EOF) {
            goto write_error;
        }
        offsets[content_object] = vice_helper_file_tell(file);
        if (offsets[content_object] < 0
            || vice_helper_file_printf(
                file, "%lu 0 obj\n<< /Length %d >>\nstream\n%sendstream\n"
                "endobj\n", (unsigned long)content_object,
                page->content_length, page->content) < 0) {
            goto write_error;
        }
        offsets[length_object] = vice_helper_file_tell(file);
        if (offsets[length_object] < 0
            || vice_helper_file_printf(
                file, "%lu 0 obj\n%lu\nendobj\n",
                (unsigned long)length_object,
                (unsigned long)compressed_output.length) < 0) {
            goto write_error;
        }
    }

    xref_offset = vice_helper_file_tell(file);
    if (xref_offset < 0 || (long long)xref_offset > 9999999999LL
        || vice_helper_file_printf(
            file, "xref\n0 %lu\n0000000000 65535 f \n",
            (unsigned long)(object_count + 1)) < 0) {
        goto write_error;
    }
    for (object_index = 1; object_index <= object_count; object_index++) {
        if (offsets[object_index] < 0
            || (long long)offsets[object_index] > 9999999999LL
            || vice_helper_file_printf(
                file, "%010ld 00000 n \n", offsets[object_index]) < 0) {
            goto write_error;
        }
    }
    if (vice_helper_file_printf(
            file,
            "trailer\n<< /Size %lu /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
            (unsigned long)(object_count + 1), xref_offset) < 0) {
        goto write_error;
    }
    result = 0;
    goto cleanup;

write_error:
    DEBUG_LOG("Failed to write PDF output.");
cleanup:
    if (file != NULL && vice_helper_file_close(file) != 0) {
        DEBUG_LOG("Failed to close PDF output file.");
        result = -1;
    }
    free(pages);
    free(offsets);
    free(row_data);
    return result;
}

static int write_gfx_pdf(void)
{
    return write_gfx_pdf_multipage();
}

#if defined(__CIRCLE__)
extern const char *prBuffer4vicedrv;
extern size_t bufferlen;

int vicepr_finish(void)
{
    int status = EXIT_SUCCESS;

    if (!vicepr_session_active) {
        return EXIT_SUCCESS;
    }

    gfx_path = vicepr_session_gfx_path;
    pdf_path = vicepr_session_pdf_path;
    if (printer_driver == NULL
        || (vicepr_session_had_input
            && !vicepr_session_last_was_formfeed
            && printer_driver->drv_formfeed(0) < 0)) {
        DEBUG_LOG("The '%s' driver failed to finish the final page.\n",
                  vicepr_session_driver);
        status = EXIT_FAILURE;
    }
    vicepr_session_shutdown_driver();
    if (vice_helper_output_error()) {
        DEBUG_LOG("Failed to capture or write printer output.");
        status = EXIT_FAILURE;
    }
    if (gfx_path != NULL && write_gfx_png() < 0) {
        status = EXIT_FAILURE;
    }
    if (pdf_path != NULL && write_gfx_pdf() < 0) {
        status = EXIT_FAILURE;
    }
    clear_vicepr_session();
    return status;
}

int vicepr_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
#if !defined(__CIRCLE__)
    uint8_t input[4096];
    size_t bytes_read;
#endif
    size_t i;
    int arg;
    char *end;
    unsigned long parsed_secondary;
    const char *driver_name = "nl10";
    unsigned int secondary_addr = 0;
    int (*init_resources)(void);
    int (*init_driver)(void);
    void (*shutdown_driver)(void);
#if !defined(__CIRCLE__)
    int saw_input = 0;
    int last_was_formfeed = 0;
#endif
    int status = EXIT_SUCCESS;

    test_mode = 0;
    stdout_enabled = 0;
    gfx_path = NULL;
    pdf_path = NULL;

    for (arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "--help") == 0) {
            DEBUG_LOG("Usage: %s [--drv nl10|mps801|mps802|mps803|1520] "
                   "[--secaddr 0-15] [--test] [--stdout] "
                   "[--gfx [path.png]] [--pdf [path.pdf]] < input-bytes\n",
                   argv[0]);
            vice_helper_puts(
                "  --drv    select nl10, mps801, mps802, mps803, or 1520 "
                "(default: nl10)");
            vice_helper_puts(
                "  --secaddr select the printer secondary address (default: 0)");
            vice_helper_puts(
                "  --test   print CBM glyphs supported by the selected driver");
            vice_helper_puts("  --stdout write the rendered raster to stdout");
            vice_helper_puts(
                "  --gfx   write PNG output to path, or printer.png if omitted");
            vice_helper_puts(
                "  --pdf   write PDF output to path, or printer.pdf if omitted");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[arg], "--test") == 0) {
            test_mode = 1;
        } else if (strcmp(argv[arg], "--stdout") == 0) {
            stdout_enabled = 1;
        } else if (strcmp(argv[arg], "--drv") == 0 && arg + 1 < argc) {
            driver_name = argv[++arg];
            if (strcmp(driver_name, "nl10") != 0
                && strcmp(driver_name, "mps801") != 0
                && strcmp(driver_name, "mps802") != 0
                && strcmp(driver_name, "mps803") != 0
                && strcmp(driver_name, "1520") != 0) {
                DEBUG_LOG("Unsupported printer driver '%s'; "
                        "choose nl10, mps801, mps802, mps803, or 1520.\n",
                        driver_name);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[arg], "--secaddr") == 0 && arg + 1 < argc) {
            errno = 0;
            parsed_secondary = strtoul(argv[++arg], &end, 10);
            if (argv[arg][0] < '0' || argv[arg][0] > '9' || *end != '\0'
                || errno == ERANGE || parsed_secondary > 15) {
                DEBUG_LOG("Invalid secondary address '%s'; "
                          "choose a value from 0 to 15.\n", argv[arg]);
                return EXIT_FAILURE;
            }
            secondary_addr = (unsigned int)parsed_secondary;
        } else if (strcmp(argv[arg], "--gfx") == 0) {
            gfx_path = "printer.png";
            if (arg + 1 < argc && argv[arg + 1][0] != '-') {
                gfx_path = argv[++arg];
            }
        } else if (strcmp(argv[arg], "--pdf") == 0) {
            pdf_path = "printer.pdf";
            if (arg + 1 < argc && argv[arg + 1][0] != '-') {
                pdf_path = argv[++arg];
            }
        } else {
            DEBUG_LOG("Usage: %s [--drv nl10|mps801|mps802|mps803|1520] "
                    "[--secaddr 0-15] [--test] "
                    "[--stdout] [--gfx [path.png]] [--pdf [path.pdf]] "
                    "< input-bytes\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (strcmp(driver_name, "nl10") == 0) {
        init_resources = drv_nl10_init_resources;
        init_driver = drv_nl10_init;
        shutdown_driver = drv_nl10_shutdown;
    } else if (strcmp(driver_name, "mps801") == 0
               || strcmp(driver_name, "mps802") == 0
               || strcmp(driver_name, "mps803") == 0) {
        init_resources = drv_mps803_init_resources;
        init_driver = drv_mps803_init;
        shutdown_driver = drv_mps803_shutdown;
    } else {
        init_resources = drv_1520_init_resources;
        init_driver = drv_1520_init;
        shutdown_driver = drv_1520_shutdown;
    }
    vice_helper_select_driver(driver_name);
#if defined(__CIRCLE__)
    if (vicepr_session_active) {
        if (strcmp(driver_name, vicepr_session_driver) != 0
            || !same_optional_string(gfx_path, vicepr_session_gfx_path)
            || !same_optional_string(pdf_path, vicepr_session_pdf_path)) {
            DEBUG_LOG("Printer settings changed before finishing the "
                      "current page.");
            return EXIT_FAILURE;
        }
        gfx_path = vicepr_session_gfx_path;
        pdf_path = vicepr_session_pdf_path;
    }
#endif
    vice_helper_set_output_options(stdout_enabled,
                                   gfx_path != NULL || pdf_path != NULL);

    if (init_resources() < 0
        || (printer_driver = vice_helper_driver()) == NULL) {
        DEBUG_LOG("Cannot register the '%s' printer driver.\n",
                driver_name);
        return EXIT_FAILURE;
    }
#if defined(__CIRCLE__)
    if (!vicepr_session_active) {
        char *session_driver = duplicate_string(driver_name);
        char *session_gfx_path = duplicate_string(gfx_path);
        char *session_pdf_path = duplicate_string(pdf_path);
        if (session_driver == NULL
            || (gfx_path != NULL && session_gfx_path == NULL)
            || (pdf_path != NULL && session_pdf_path == NULL)) {
            free(session_driver);
            free(session_gfx_path);
            free(session_pdf_path);
            DEBUG_LOG("Cannot retain printer session settings.");
            return EXIT_FAILURE;
        }
        if (init_driver() < 0
            || printer_driver->drv_open(0, DRIVER_FIRST_OPEN) < 0
            || printer_driver->drv_flush(0, secondary_addr) < 0
            || printer_driver->drv_open(0, secondary_addr) < 0) {
            DEBUG_LOG("The '%s' driver failed to initialize.\n",
                      driver_name);
            shutdown_driver();
            free(session_driver);
            free(session_gfx_path);
            free(session_pdf_path);
            vice_helper_free_gfx_pixels();
            return EXIT_FAILURE;
        }
        vicepr_session_driver = session_driver;
        vicepr_session_gfx_path = session_gfx_path;
        vicepr_session_pdf_path = session_pdf_path;
        vicepr_session_shutdown_driver = shutdown_driver;
        vicepr_session_active = 1;
    } else if (printer_driver->drv_open(0, secondary_addr) < 0) {
        DEBUG_LOG("The '%s' driver failed to open secondary address %u.\n",
                  driver_name, secondary_addr);
        return EXIT_FAILURE;
    }
#else
    if (init_driver() < 0
        || printer_driver->drv_open(0, DRIVER_FIRST_OPEN) < 0
        || printer_driver->drv_flush(0, secondary_addr) < 0
        || printer_driver->drv_open(0, secondary_addr) < 0) {
        DEBUG_LOG("The '%s' driver failed to initialize.\n",
                driver_name);
        status = EXIT_FAILURE;
    }
#endif
    if (status == EXIT_SUCCESS && test_mode) {
        unsigned int character, printed = 0;

        for (character = 0; character <= 255; character++) {
            /* The NL10 protocol reserves these ranges for control codes. */
            if (character < 0x20 || (character >= 0x80 && character < 0xa0)) {
                continue;
            }
            if (printer_driver->drv_putc(
                    0, secondary_addr, (uint8_t)character) < 0) {
                DEBUG_LOG("The '%s' driver failed during the "
                        "character test.\n", driver_name);
                status = EXIT_FAILURE;
                break;
            }
            printed++;
            if (printed % TEST_GLYPHS_PER_LINE == 0
                && printed < TEST_GLYPH_COUNT
                && printer_driver->drv_putc(0, secondary_addr, '\r') < 0) {
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
#if !defined(__CIRCLE__)
        saw_input = 1;
#endif
    } else if (status == EXIT_SUCCESS) {
#if !defined(__CIRCLE__)        
        while ((bytes_read = vice_helper_file_read(
                    input, 1, sizeof(input), vice_helper_stdin())) > 0) {
            for (i = 0; i < bytes_read; i++) {
                if (printer_driver->drv_putc(
                        0, secondary_addr, input[i]) < 0) {
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
        if (vice_helper_file_error(vice_helper_stdin())) {
            DEBUG_LOG("Failed to read printer data from stdin.");
            status = EXIT_FAILURE;
        } else 
#else
        for (i = 0; i < bufferlen; i++) {
            if (printer_driver->drv_putc(
                    0, secondary_addr, prBuffer4vicedrv[i]) < 0)
            {
                DEBUG_LOG("The '%s' driver failed to process input.", driver_name);
                    status = EXIT_FAILURE;
                    break;
            }
            vicepr_session_had_input = 1;
            vicepr_session_last_was_formfeed =
                prBuffer4vicedrv[i] == '\f';
        }
#endif       
#if !defined(__CIRCLE__)
        if (saw_input && !last_was_formfeed
                   && printer_driver->drv_formfeed(0) < 0) {
            DEBUG_LOG("The '%s' driver failed to finish the final "
                    "page.\n", driver_name);
            status = EXIT_FAILURE;
        }
#endif
    }

#if defined(__CIRCLE__)
    if (vice_helper_output_error()) {
        DEBUG_LOG("Failed to capture or write printer output.");
        status = EXIT_FAILURE;
    }
    if (gfx_path != NULL && write_gfx_png() < 0) {
        status = EXIT_FAILURE;
    }
    if (pdf_path != NULL && write_gfx_pdf() < 0) {
        status = EXIT_FAILURE;
    }
    return status;
#else
    shutdown_driver();
    if (vice_helper_output_error()) {
        DEBUG_LOG("Failed to capture or write printer output.");
        status = EXIT_FAILURE;
    }
    if (gfx_path != NULL && write_gfx_png() < 0) {
        status = EXIT_FAILURE;
    }
    if (pdf_path != NULL && write_gfx_pdf() < 0) {
        status = EXIT_FAILURE;
    }
    vice_helper_free_gfx_pixels();
    return status;
#endif
}
