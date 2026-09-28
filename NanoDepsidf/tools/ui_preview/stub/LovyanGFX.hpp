#pragma once
// Host stand-in for the two LovyanGFX font types the UI code touches -- same field layout
// as lgfx/v1/lgfx_fonts.hpp, so the generated font .cpp files compile unchanged.
#include <stdint.h>

namespace lgfx {
struct GFXglyph {
    uint32_t bitmapOffset;
    uint8_t width, height;
    uint8_t xAdvance;
    int8_t xOffset, yOffset;
};
struct GFXfont {
    uint8_t *bitmap;
    GFXglyph *glyph;
    uint16_t first, last;
    uint8_t yAdvance;
    constexpr GFXfont(uint8_t *b, GFXglyph *g, uint16_t f, uint16_t l, uint8_t y)
        : bitmap(b), glyph(g), first(f), last(l), yAdvance(y) {}
};
} // namespace lgfx
