#ifndef PRINTER_ATTR_H
#define PRINTER_ATTR_H

#include <stdint.h>

typedef struct PrinterAttr {
    uint32_t dpi_x;
    uint32_t dpi_y;
    uint32_t margin_top;    /* Hundredths of a millimeter. */
    uint32_t margin_left;
    uint32_t margin_right;
    uint32_t margin_bottom;
} PrinterAttr;

#endif
