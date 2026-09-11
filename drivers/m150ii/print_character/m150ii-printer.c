/*
 *  MNXYG
 *
 * SPX-License-Identifer: Apache-2.0
*/

#include "m150ii-printer.h"

/*add font file*/
#if defined(CONFIG_M150II_USE_FONT_ASCII_5X7_FONT)
#include "ascii-5x7-font.h"
#endif
/*add dependency*/
#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stdarg.h>

LOG_MODULE_DECLARE(M150II, CONFIG_EPSON_M150II_LOG_LEVEL);
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
int m150_printf (const struct device *dev, enum font_type_t font, char *format, ...)
{
    /*check devie is ready*/
    if (!device_is_ready(dev))
    {
        /*error : device is not ready*/
        LOG_ERR("Device is not ready");
        return -ENODEV;
    }
    int ret = 0;
    char buff[CONFIG_M150II_CHARACTER_BUFF_SIZE];
    int buff_count;
    va_list aptr;
    va_start(aptr, format);
    buff_count = vsprintf(buff, format, aptr);
    va_end(aptr);
    if (buff_count < 0)
    {
        /*log error : write buff faile*/
        LOG_ERR("buff size to small");
        ret = -ENOMEM;
        return ret;
    }
    LOG_DBG("strings format ok:\n%s",buff);
    /*check font type*/
    unsigned char line_sigle = 0;
    unsigned char font_width = 0;
    switch (font)
    {
#if defined(CONFIG_M150II_USE_FONT_ASCII_5X7_FONT)
    case FONT_TYPE_ASCII_5X7:
        /*7+2 space*/
        line_sigle = ASCII_5X7_FONT_HEIGHT + 2;
        font_width = ASCII_5X7_FONT_WIDTH + 1;
        break;
#endif
    default:
        /*error : no this type*/
        LOG_ERR("not this font type");
        ret = -ENOTSUP;
        return ret;
        break;
    }
    const unsigned char sigle_line_font_count = 96 / font_width;
    int line_conter = line_sigle;
    unsigned char cow_conter = 0;
    /*counter line*/
    for (int i = 0; i < buff_count; i++)
    {
        if (buff[i] == 0X0A || buff[i] == 0X0D)
        {
            cow_conter = 0;
            line_conter += line_sigle;
            continue;
        }
        else if (cow_conter >= sigle_line_font_count)
        {
            cow_conter = 0;
            line_conter += line_sigle;
        }
        LOG_DBG("cow_conter = %d, i = %d, line_conter = %d",cow_conter,i,line_conter);
        cow_conter++;
    }
    /*create a new drow buff*/
    uint8_t buf[12 * (line_conter - 1)];
    memset(buf,0x00,sizeof(buf));
    /*add font to drow*/
    line_conter = 0;
    cow_conter = 0;
    for (int i = 0; i < buff_count; i++)
    {
        if (buff[i] == 0x0A || buff[i] == 0X0D)
        {
            /*next line*/
            cow_conter = 0;
            line_conter += line_sigle;
            continue;
        }
        else if (cow_conter >= sigle_line_font_count)
        {
            cow_conter = 0;
            line_conter += line_sigle;
        }
        /*add font to line*/
        /*check font*/
        switch (font)
        {
#if defined(CONFIG_M150II_USE_FONT_ASCII_5X7_FONT)
        case FONT_TYPE_ASCII_5X7:
            /*check font data*/
            if (buff[i] < ASCII_5X7_FONT_MIN_CODE || buff[i] > ASCII_5X7_FONT_MAX_CODE)
            {
                buff[i] = ASCII_5X7_FONT_MAX_CODE + 1;
            }
            /*add font to position*/
            for (size_t a = 0; a < ASCII_5X7_FONT_HEIGHT; a++)
            {
                char align_offset_bit = (cow_conter * font_width) % 8; /*x coordinate of 8*/
                char align_offset_byte = (cow_conter * font_width) / 8; /*x coordinate mine 8*/
                if (align_offset_bit)
                {
                    buf[((line_conter + a) * 12) + align_offset_byte] |= ascii_5x7_font[buff[i] - ASCII_5X7_FONT_MIN_CODE][a] >> align_offset_bit;
                    buf[((line_conter + a) * 12) + align_offset_byte + 1] |= ascii_5x7_font[buff[i] - ASCII_5X7_FONT_MIN_CODE][a] << (8 - align_offset_bit);
                }
                else
                {
                    buf[((line_conter + a) * 12) + align_offset_byte] |= ascii_5x7_font[buff[i] - ASCII_5X7_FONT_MIN_CODE][a];
                }
            }
            break;
#endif
        default:
            /*no this type*/
            LOG_ERR("not this font type");
            ret = -ENOTSUP;
            goto exit;
            break;
        }
        cow_conter++;
    }
    /*write to device*/
    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(buf),
        .width = 96,
        .pitch = 96,
        .height = sizeof(buf) / 12,
        .frame_incomplete = false,
    };
    ret = display_write(dev, 0, 0, &desc, buf);
    if (ret)
    {
        /*error code : cant not display to printer*/
        LOG_ERR("cant not write printer code %d",ret);
    }
    exit:
    return ret;
}