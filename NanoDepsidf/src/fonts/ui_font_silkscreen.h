// Silkscreen (OFL license, google/fonts ofl/silkscreen), ASCII 0x20-0x7E, 1bpp -- true
// monochrome pixel-art font, no anti-aliasing. LovyanGFX GFXfont format, repacked
// pixel-for-pixel from the earlier LVGL conversion (data in ui_font_silkscreen.cpp).
//
// Regular weight only, used everywhere: 16px for the main screen + menu rows, 8px for the
// Main Screen's F1/F3/F4 cheat-sheet tags. A bold 16px weight for the selected menu row was
// tried twice and rejected on hardware both times -- keep weight uniform.
#pragma once

#include <LovyanGFX.hpp>

extern const lgfx::GFXfont ui_font_silkscreen_16_regular; // line height 18px
extern const lgfx::GFXfont ui_font_silkscreen_8_regular;  // line height 9px
