#pragma once

#include <stdint.h>
#include <stdbool.h>

// Cross-core UI state: control_task.c (Core 0) produces the input-side values,
// display_task.cpp (Core 1) consumes them on its own redraw timer. Plain atomics -- the
// display only ever wants the *latest* value each time it redraws, never a history of every
// intermediate change, so there's nothing to lose by overwriting.
//
// Phase 8: the mock menu's active/selection fields this file used to carry are gone -- the
// real menu (`menu.c`) owns its own state and exposes it via menu_get_render_snapshot()
// instead, mutex-protected rather than atomics (a menu row is formatted text, not a scalar).

void ui_state_init(void);

// Producer side (Core 0)
void ui_state_set_detent(int32_t wrapped_index); // 0..(num_detents-1) -- already wrapped
                                                  // by the caller, this module doesn't
                                                  // know num_detents

// Pixel UI: which of F1..F4 are held right now (bit 0 = F1 ... bit 3 = F4), for the Main
// Screen's keycap press feedback. Also the display's "someone touched it" signal, together
// with detent changes, for the attract-mode idle timer.
#define UI_BTN_F1 (1u << 0)
#define UI_BTN_F2 (1u << 1)
#define UI_BTN_F3 (1u << 2)
#define UI_BTN_F4 (1u << 3)
void ui_state_set_buttons(uint8_t held_mask);

// The knob's total (unwrapped) travel since boot, in 1e-4 rad units, every control tick --
// for visuals that follow the knob continuously rather than per detent (APP-mode 3D shape).
// int32 at 1e-4 rad covers ~34,000 turns before wrapping; consumers only use differences.
void ui_state_set_knob_angle(int32_t angle_1e4_rad);

// Consumer side (Core 1)
int32_t ui_state_get_knob_angle(void);
int32_t ui_state_get_detent(void); // no longer drawn, but a change means "knob turned"
uint8_t ui_state_get_buttons(void);

// Pixel UI: the USB personality this boot actually came up with (main.c decides it before
// any task starts; the F3+F4 failsafe can override the saved setting). The Main Screen status
// strip and the Boot mode screen's IN USE marker show this, not the saved setting.
void ui_state_set_usb_serial_active(bool serial);
bool ui_state_get_usb_serial_active(void);

// Pixel UI: set by the display (Core 1) while the attract animation runs, read by
// control_task.c (Core 0) so the button press that wakes the screen is swallowed instead of
// also opening the menu.
void ui_state_set_screensaver(bool active);
bool ui_state_get_screensaver(void);
