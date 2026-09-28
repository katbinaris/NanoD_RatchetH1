#pragma once
// Host stand-in for the LGFX sprite: just the calls ui_gfx.cpp makes (fillRect with an
// RGB888 color, clip rect), drawing into a 240x240 RGB buffer.
#include <stdint.h>
#include <LovyanGFX.hpp>

class LGFX_Sprite {
public:
    static constexpr int W = 240, H = 240;
    uint32_t px[W * H] = {};

    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb) {
        int x0 = x < cx ? cx : x, y0 = y < cy ? cy : y;
        int x1 = x + w > cx + cw ? cx + cw : x + w, y1 = y + h > cy + ch ? cy + ch : y + h;
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++) px[yy * W + xx] = rgb;
    }
    void setClipRect(int32_t x, int32_t y, int32_t w, int32_t h) { cx = x; cy = y; cw = w; ch = h; }
    void clearClipRect() { cx = 0; cy = 0; cw = W; ch = H; }
    void clear() { for (auto &p : px) p = 0; }

private:
    int cx = 0, cy = 0, cw = W, ch = H;
};
