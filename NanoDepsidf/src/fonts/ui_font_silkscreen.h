// Silkscreen (OFL license, google/fonts ofl/silkscreen), converted to a static LVGL bitmap
// font via lv_font_conv, ASCII 0x20-0x7E only, 16px/1bpp -- true monochrome, no
// anti-aliasing, since Silkscreen is a pixel-art font already drawn on a grid rather than
// an outline font being rasterized down (which is what made the earlier Sora conversion
// look soft at 14px). Replaces ui_font_sora_14_*. Regular only, used everywhere (main
// screen + menu roller, selected row included) -- a Bold weight existed briefly for the
// roller's selected row, dropped by request in favor of all-regular.
#pragma once

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_silkscreen_16_regular);
