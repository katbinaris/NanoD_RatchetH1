#pragma once
// MUSIC's cover on a record (user_prefs.h cover_style_t): RECORD (the glass is the record, the
// cover its label), SLIDE (the cover as a sleeve, the record slides out of its side) and BLEED
// (a big sleeve that slides off the glass as the record comes out).
//
// The record is the one place outside the cover that uses more greys than the palette: a
// 10-step ramp with 4x4 ordered dither, in whole pixels (PIXEL_ART.md section 2). Its shimmer
// -- groove grain and dust that turn with the record and catch a light that stays put -- is
// what shows the spin.
//
// Cheap per frame: the grooves and the light are drawn once per layout into `base`, 4 bits a
// pixel (an index into the ramp); a frame expands them, redraws only the light's wedge (the
// grain moves through it), places the dust and rotates the label by stepping along each row.
// The small per-pixel tables are in internal RAM; PSRAM holds only what is read in order.

#include <stddef.h>
#include <stdint.h>

namespace ui {

// Buffers the caller owns (PSRAM on the device), RGB565 big-endian where they hold pixels.
struct VinylMem {
    uint16_t *polar;  // VINYL_POLAR_PX: distance and angle for one quadrant, made once
    uint8_t *base;    // VINYL_BASE_BYTES: the current layout's record without its label, 4 bits a pixel
    uint16_t *label;  // VINYL_LABEL_PX: the cover, averaged to the label's size
    uint16_t *sleeve; // VINYL_SLEEVE_PX: the cover, averaged to the sleeve's size, with ring wear
    void *tables;     // VINYL_TABLES_BYTES: the record's spans and where the light can reach
};
constexpr int VINYL_POLAR_PX = 118 * 118;
constexpr int VINYL_BASE_BYTES = 118 * 236;
constexpr int VINYL_LABEL_PX = 108 * 108;
constexpr int VINYL_SLEEVE_PX = 150 * 150;
constexpr int VINYL_TABLES_BYTES = 6 * 1024;

void vinyl_bind(const VinylMem &m);
// The cover (240x240, RGB565 big-endian, not darkened) for this layout: makes its label and
// sleeve, and the record's grooves when the layout changed. Call when either changes.
void vinyl_prepare(int style, const uint16_t *cover_be);
// The layout at one moment into the bound frame: `theta` the record's angle (radians,
// clockwise), `out` how far it is out of its sleeve (0..1, a little over while it overshoots).
void vinyl_draw(int style, float theta, float out);

} // namespace ui
