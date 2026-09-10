/*
 *  MNXYG
 *
 * SPX-License-Identifer: Apache-2.0
*/

#if !defined(M150II_PRINTER_H)
#define M150II_PRINTER_H

#include <zephyr/drivers/display.h>


enum font_type_t
{
#if defined(CONFIG_M150II_USE_FONT_ASCII_5X7_FONT)
    FONT_TYPE_ASCII_5X7,
#else
message "error not enable any font, not font can use for printer"
#endif
};

/*
 *  @brief this function use for format data, 
 *      chanslate to drow buff, and printer to
 *      EPSON m150ii micro dot printer
 *  @param  dev device handle
 *          font font type
 *          format string of input
 *  @return 0 for success
 *          error code for anther
*/
int m150_printf (const struct device *dev, enum font_type_t font, char *format, ...);

#endif // M150II_PRINTER_H
