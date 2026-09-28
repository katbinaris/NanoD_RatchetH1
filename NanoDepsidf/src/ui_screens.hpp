#pragma once
// Pixel UI screens (DEVELOPMENT_PLAN.md "Pixel UI"), ported from the approved mockups. Each
// draws one full screen into the bound sprite from plain inputs -- no state of their own;
// display_task.cpp owns timing, transitions and the animation state they're handed.

#include <stdint.h>
#include "haptic_params.h"
extern "C" {
#include "menu.h"
#include "audio_trigger.h"
#include "boot_mode.h"
}

namespace ui {

struct MainInputs {
    bool usb_serial;          // this boot's USB personality (status strip)
    audio_click_timbre_t tone;
    menu_hid_type_t mode;     // knob's job: keyboard / mouse / MIDI
    haptic_type_t feel;
    uint8_t buttons_held;     // UI_BTN_* -- lit keycaps
    const uint8_t *icon;      // HID-uploaded 48x48 RGB565 (big-endian) icon, or nullptr
};
void draw_main(const MainInputs &in);

// Top-level menu: the hardware-confirmed scrolling list, text only.
void draw_menu_list(const menu_render_snapshot_t &snap, float scroll_px);

struct OrbitInputs {
    haptic_type_t feel;
    uint32_t t_ms;     // FEEL animation clock
    int morph_from;    // previous feel type while morphing, -1 otherwise
    float morph_blend; // 0..1
    bool blink_on;     // "F2 SAVE" blink phase
};
void draw_orbit(const menu_render_snapshot_t &snap, const OrbitInputs &in);

struct HidInputs {
    menu_hid_type_t type;
    float slide_px;    // carousel offset while sliding to `type`, 0 at rest
    bool blink_on;
};
void draw_hid(const menu_render_snapshot_t &snap, const HidInputs &in);

void draw_boot_mode(const menu_render_snapshot_t &snap, boot_usb_mode_t selected, bool serial_in_use, bool blink_on);

void draw_saved_toast();

} // namespace ui
