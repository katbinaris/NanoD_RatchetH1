#pragma once
// Pixel UI effects: the loading screen and the attract animations. All time-driven and
// stateless per frame (a pure function of elapsed ms), drawn through ui_gfx.

#include <stdint.h>

namespace ui {

void fx_init(); // one-time tables (logo particles, plasma lookups)

// Loading screen "Big bang": a spark swells and bursts, the logo's own pixels scatter, then
// fly home and lock in; the maker line types in and a block bar fills.
constexpr uint32_t BOOT_ANIM_MS = 3400;
void fx_boot(uint32_t elapsed_ms);

// Attract animation: dark plasma rippling out of the QUADRA lettering in the middle -- or, in
// APP mode, out of the active profile's 48x48 icon (drawn at 2x). (Warp was dropped by
// request; Plasma is the only one.)
void fx_attract(uint32_t t_ms, const uint8_t *icon48 = nullptr);

} // namespace ui
