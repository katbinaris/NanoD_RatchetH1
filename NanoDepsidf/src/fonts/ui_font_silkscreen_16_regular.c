/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --font Silkscreen-Regular.ttf -r 0x20-0x7E --size 16 --format lvgl --bpp 1 --no-compress --no-prefilter --autohint-off --force-fast-kern-format --lv-include lvgl.h -o ui_font_silkscreen_16_regular.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef UI_FONT_SILKSCREEN_16_REGULAR
#define UI_FONT_SILKSCREEN_16_REGULAR 1
#endif

#if UI_FONT_SILKSCREEN_16_REGULAR

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf0, 0xf0,

    /* U+0022 "\"" */
    0xcf, 0x3c, 0xf3,

    /* U+0023 "#" */
    0x33, 0xc, 0xcf, 0xff, 0xff, 0x33, 0xc, 0xcf,
    0xff, 0xff, 0x33, 0xc, 0xc0,

    /* U+0024 "$" */
    0xc, 0xc, 0x3f, 0x3f, 0xc0, 0xc0, 0x3c, 0x3c,
    0x3, 0x3, 0xfc, 0xfc, 0x30, 0x30,

    /* U+0025 "%" */
    0xf3, 0x3c, 0xcf, 0x33, 0xcc, 0xc, 0x3, 0x3,
    0x3c, 0xcf, 0x33, 0xcc, 0xf0,

    /* U+0026 "&" */
    0xc, 0xc, 0x3f, 0x3f, 0xc0, 0xc0, 0x3c, 0x3c,
    0xc0, 0xc0, 0x3f, 0x3f, 0xc, 0xc,

    /* U+0027 "'" */
    0xff,

    /* U+0028 "(" */
    0x33, 0xcc, 0xcc, 0xcc, 0x33,

    /* U+0029 ")" */
    0xcc, 0x33, 0x33, 0x33, 0xcc,

    /* U+002A "*" */
    0xc, 0x3, 0xc, 0xcf, 0x33, 0x3f, 0xf, 0xcc,
    0xcf, 0x33, 0xc, 0x3, 0x0,

    /* U+002B "+" */
    0xc, 0x3, 0x0, 0xc0, 0x30, 0xff, 0xff, 0xf0,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+002C "," */
    0x33, 0xcc,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0xc, 0x30, 0xc3, 0x30, 0xcc, 0x30, 0xc3, 0x0,

    /* U+0030 "0" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0031 "1" */
    0xf3, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0xff, 0xf0,

    /* U+0032 "2" */
    0xfc, 0xfc, 0x3, 0x3, 0x3c, 0x3c, 0xc0, 0xc0,
    0xff, 0xff,

    /* U+0033 "3" */
    0xfc, 0xfc, 0x3, 0x3, 0x3c, 0x3c, 0x3, 0x3,
    0xfc, 0xfc,

    /* U+0034 "4" */
    0xcc, 0xcc, 0xcc, 0xcc, 0xff, 0xff, 0xc, 0xc,
    0xc, 0xc,

    /* U+0035 "5" */
    0xff, 0xff, 0xc0, 0xc0, 0xfc, 0xfc, 0x3, 0x3,
    0xfc, 0xfc,

    /* U+0036 "6" */
    0x3c, 0x3c, 0xc0, 0xc0, 0xfc, 0xfc, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0037 "7" */
    0xff, 0xff, 0x3, 0x3, 0xc, 0xc, 0x30, 0x30,
    0x30, 0x30,

    /* U+0038 "8" */
    0x3c, 0x3c, 0xc3, 0xc3, 0x3c, 0x3c, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0039 "9" */
    0x3c, 0x3c, 0xc3, 0xc3, 0x3f, 0x3f, 0x3, 0x3,
    0x3c, 0x3c,

    /* U+003A ":" */
    0xf0, 0xf0,

    /* U+003B ";" */
    0x33, 0x0, 0x33, 0xcc,

    /* U+003C "<" */
    0xc, 0x33, 0xc, 0xc3, 0x3, 0xc, 0xc, 0x30,

    /* U+003D "=" */
    0xff, 0xf0, 0x0, 0xff, 0xf0,

    /* U+003E ">" */
    0xc3, 0x3, 0xc, 0xc, 0x33, 0xc, 0xc3, 0x0,

    /* U+003F "?" */
    0xfc, 0xfc, 0x3, 0x3, 0x3c, 0x3c, 0x0, 0x0,
    0x30, 0x30,

    /* U+0040 "@" */
    0x3f, 0xf, 0xcc, 0xcf, 0x33, 0xcf, 0x33, 0xcc,
    0x3, 0x0, 0x3f, 0xf, 0xc0,

    /* U+0041 "A" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+0042 "B" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xfc, 0xfc,

    /* U+0043 "C" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc0, 0xc0, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0044 "D" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xfc, 0xfc,

    /* U+0045 "E" */
    0xff, 0xfc, 0x30, 0xff, 0xfc, 0x30, 0xff, 0xf0,

    /* U+0046 "F" */
    0xff, 0xfc, 0x30, 0xff, 0xfc, 0x30, 0xc3, 0x0,

    /* U+0047 "G" */
    0x3f, 0x3f, 0xc0, 0xc0, 0xcf, 0xcf, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0048 "H" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+0049 "I" */
    0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+004B "K" */
    0xc3, 0xc3, 0xcc, 0xcc, 0xf0, 0xf0, 0xcc, 0xcc,
    0xc3, 0xc3,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xff, 0xf0,

    /* U+004D "M" */
    0xc0, 0xf0, 0x3f, 0x3f, 0xcf, 0xcc, 0xf3, 0x3c,
    0xf, 0x3, 0xc0, 0xf0, 0x30,

    /* U+004E "N" */
    0xc0, 0xf0, 0x3f, 0xf, 0xc3, 0xcc, 0xf3, 0x3c,
    0x3f, 0xf, 0xc0, 0xf0, 0x30,

    /* U+004F "O" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0050 "P" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xfc, 0xfc, 0xc0, 0xc0,
    0xc0, 0xc0,

    /* U+0051 "Q" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c, 0x3, 0x3,

    /* U+0052 "R" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xfc, 0xfc, 0xcc, 0xcc,
    0xc3, 0xc3,

    /* U+0053 "S" */
    0x3f, 0x3f, 0xc0, 0xc0, 0x3c, 0x3c, 0x3, 0x3,
    0xfc, 0xfc,

    /* U+0054 "T" */
    0xff, 0xf3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc0,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0056 "V" */
    0xc0, 0xf0, 0x3c, 0xf, 0x3, 0x33, 0xc, 0xc3,
    0x30, 0xcc, 0xc, 0x3, 0x0,

    /* U+0057 "W" */
    0xc0, 0xf0, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcf, 0x33, 0x33, 0xc, 0xc0,

    /* U+0058 "X" */
    0xc0, 0xf0, 0x33, 0x30, 0xcc, 0xc, 0x3, 0x3,
    0x30, 0xcc, 0xc0, 0xf0, 0x30,

    /* U+0059 "Y" */
    0xc0, 0xf0, 0x33, 0x30, 0xcc, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+005A "Z" */
    0xff, 0xf0, 0xc3, 0x30, 0xcc, 0x30, 0xff, 0xf0,

    /* U+005B "[" */
    0xff, 0xcc, 0xcc, 0xcc, 0xff,

    /* U+005C "\\" */
    0xc3, 0xc, 0x30, 0x30, 0xc0, 0xc3, 0xc, 0x30,

    /* U+005D "]" */
    0xff, 0x33, 0x33, 0x33, 0xff,

    /* U+005E "^" */
    0x30, 0xcc, 0xf3,

    /* U+005F "_" */
    0xff, 0xff,

    /* U+0060 "`" */
    0xcc, 0x33,

    /* U+0061 "a" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+0062 "b" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xfc, 0xfc,

    /* U+0063 "c" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc0, 0xc0, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0064 "d" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xfc, 0xfc,

    /* U+0065 "e" */
    0xff, 0xfc, 0x30, 0xff, 0xfc, 0x30, 0xff, 0xf0,

    /* U+0066 "f" */
    0xff, 0xfc, 0x30, 0xff, 0xfc, 0x30, 0xc3, 0x0,

    /* U+0067 "g" */
    0x3f, 0x3f, 0xc0, 0xc0, 0xcf, 0xcf, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0068 "h" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+0069 "i" */
    0xff, 0xff, 0xf0,

    /* U+006A "j" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+006B "k" */
    0xc3, 0xc3, 0xcc, 0xcc, 0xf0, 0xf0, 0xcc, 0xcc,
    0xc3, 0xc3,

    /* U+006C "l" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xff, 0xf0,

    /* U+006D "m" */
    0xc0, 0xf0, 0x3f, 0x3f, 0xcf, 0xcc, 0xf3, 0x3c,
    0xf, 0x3, 0xc0, 0xf0, 0x30,

    /* U+006E "n" */
    0xc0, 0xf0, 0x3f, 0xf, 0xc3, 0xcc, 0xf3, 0x3c,
    0x3f, 0xf, 0xc0, 0xf0, 0x30,

    /* U+006F "o" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0070 "p" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xfc, 0xfc, 0xc0, 0xc0,
    0xc0, 0xc0,

    /* U+0071 "q" */
    0x3c, 0x3c, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c, 0x3, 0x3,

    /* U+0072 "r" */
    0xfc, 0xfc, 0xc3, 0xc3, 0xfc, 0xfc, 0xcc, 0xcc,
    0xc3, 0xc3,

    /* U+0073 "s" */
    0x3f, 0x3f, 0xc0, 0xc0, 0x3c, 0x3c, 0x3, 0x3,
    0xfc, 0xfc,

    /* U+0074 "t" */
    0xff, 0xf3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc0,

    /* U+0075 "u" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x3c, 0x3c,

    /* U+0076 "v" */
    0xc0, 0xf0, 0x3c, 0xf, 0x3, 0x33, 0xc, 0xc3,
    0x30, 0xcc, 0xc, 0x3, 0x0,

    /* U+0077 "w" */
    0xc0, 0xf0, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcf, 0x33, 0x33, 0xc, 0xc0,

    /* U+0078 "x" */
    0xc0, 0xf0, 0x33, 0x30, 0xcc, 0xc, 0x3, 0x3,
    0x30, 0xcc, 0xc0, 0xf0, 0x30,

    /* U+0079 "y" */
    0xc0, 0xf0, 0x33, 0x30, 0xcc, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+007A "z" */
    0xff, 0xf0, 0xc3, 0x30, 0xcc, 0x30, 0xff, 0xf0,

    /* U+007B "{" */
    0x3c, 0xf3, 0xc, 0xc3, 0x3, 0xc, 0x3c, 0xf0,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0xf3, 0xc3, 0xc, 0xc, 0x33, 0xc, 0xf3, 0xc0,

    /* U+007E "~" */
    0x33, 0x33, 0xcc, 0xcc
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 128, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 160, .box_w = 6, .box_h = 4, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 7, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 192, .box_w = 8, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 34, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 192, .box_w = 8, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 61, .adv_w = 96, .box_w = 2, .box_h = 4, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 62, .adv_w = 128, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 128, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 85, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 128, .box_w = 4, .box_h = 4, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 100, .adv_w = 160, .box_w = 6, .box_h = 2, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 102, .adv_w = 96, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 111, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 209, .adv_w = 96, .box_w = 2, .box_h = 6, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 211, .adv_w = 128, .box_w = 4, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 215, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 223, .adv_w = 160, .box_w = 6, .box_h = 6, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 228, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 279, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 299, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 335, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 338, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 379, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 402, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 412, .adv_w = 192, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 424, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 434, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 444, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 462, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 488, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 501, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 128, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 535, .adv_w = 128, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 540, .adv_w = 160, .box_w = 6, .box_h = 4, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 543, .adv_w = 192, .box_w = 8, .box_h = 2, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 545, .adv_w = 128, .box_w = 4, .box_h = 4, .ofs_x = 2, .ofs_y = 12},
    {.bitmap_index = 547, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 557, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 567, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 595, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 626, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 636, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 654, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 667, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 680, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 690, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 700, .adv_w = 192, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 712, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 722, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 732, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 740, .adv_w = 192, .box_w = 8, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 750, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 763, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 776, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 789, .adv_w = 224, .box_w = 10, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 802, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 810, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 818, .adv_w = 96, .box_w = 2, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 822, .adv_w = 160, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 830, .adv_w = 192, .box_w = 8, .box_h = 4, .ofs_x = 2, .ofs_y = 6}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_silkscreen_16_regular = {
#else
lv_font_t ui_font_silkscreen_16_regular = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 18,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_SILKSCREEN_16_REGULAR*/

