/*******************************************************************************
 * Size: 28 px
 * Bpp: 1
 * Opts: --bpp 1 --size 28 --font /Users/karolmalota/SquareLine/assets/idlecat-Regular-2.otf -o /Users/karolmalota/SquareLine/assets/ui_font_idleArrow28.c --format lvgl -r 0x46-0x47 --no-compress --no-prefilter
 ******************************************************************************/

#include "../ui.h"

#ifndef UI_FONT_IDLEARROW28
#define UI_FONT_IDLEARROW28 1
#endif

#if UI_FONT_IDLEARROW28

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0046 "F" */
    0x0, 0x3, 0x0, 0x0, 0x0, 0x30, 0x0, 0x0,
    0xf, 0xc0, 0x0, 0x0, 0xfc, 0x0, 0x0, 0xf,
    0xf0, 0x0, 0x0, 0xff, 0x0, 0x3f, 0xff, 0xfc,
    0x3, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xf,
    0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xcf, 0xff,
    0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xfc, 0xff,
    0xff, 0xff, 0xf, 0xff, 0xff, 0xf0, 0x3f, 0xff,
    0xfc, 0x3, 0xff, 0xff, 0xc0, 0x0, 0xf, 0xf0,
    0x0, 0x0, 0xff, 0x0, 0x0, 0xf, 0xc0, 0x0,
    0x0, 0xfc, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
    0x30, 0x0,

    /* U+0047 "G" */
    0x0, 0x3, 0x0, 0x0, 0x0, 0x30, 0x0, 0x0,
    0xc, 0xc0, 0x0, 0x0, 0xcc, 0x0, 0x0, 0x0,
    0x30, 0x0, 0x0, 0x3, 0x0, 0x30, 0x0, 0xc,
    0x3, 0x0, 0x0, 0xc0, 0xc0, 0x0, 0x3, 0xc,
    0x0, 0x0, 0x30, 0xc0, 0x0, 0x0, 0xcc, 0x0,
    0x0, 0xc, 0xc0, 0x0, 0x0, 0x3c, 0x0, 0x0,
    0x3, 0xc0, 0x0, 0x0, 0x3c, 0x0, 0x0, 0x3,
    0xc0, 0x0, 0x0, 0xcc, 0x0, 0x0, 0xc, 0xc0,
    0x0, 0x3, 0xc, 0x0, 0x0, 0x30, 0x30, 0x0,
    0xc, 0x3, 0x0, 0x0, 0xc0, 0x0, 0x0, 0x30,
    0x0, 0x0, 0x3, 0x0, 0x0, 0xc, 0xc0, 0x0,
    0x0, 0xcc, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
    0x30, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 448, .box_w = 28, .box_h = 28, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 98, .adv_w = 448, .box_w = 28, .box_h = 28, .ofs_x = 0, .ofs_y = -4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 70, .range_length = 2, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LV_VERSION_CHECK(8, 0, 0)
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
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
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LV_VERSION_CHECK(9, 0, 0)
const lv_font_t ui_font_idleArrow28 = {
#else
lv_font_t ui_font_idleArrow28 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 28,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if UI_FONT_IDLEARROW28*/

