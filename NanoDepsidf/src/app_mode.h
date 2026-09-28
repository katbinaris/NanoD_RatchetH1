#pragma once

#include <stdbool.h>
#include <stdint.h>

// APP mode (HID type "APP"): F1-F4 become application controls instead of menu keys, for
// one hardcoded profile -- Plasticity for this first test (DEVELOPMENT_PLAN.md "APP mode").
//
//   knob alone / F1 held + turn  zoom   (Ctrl + middle-button drag, vertical: continuous)
//   F2 held + turn               orbit  (orbit mouse button held + horizontal mouse move)
//   F3 press                     undo   (Cmd+Z -- one keyboard shortcut, see app_mode.c)
//   F4 held + turn               pan    (pan mouse button held + horizontal mouse move)
//   F4 held ~0.7s, knob still    opens the menu (F3/F1/F4 work as usual inside it)
//
// A drag's mouse button only goes down once the knob actually moves, so holding F4 without
// turning never sends anything -- that's what lets the long press mean "menu". Turning with
// no key held starts a zoom drag by itself, which lets go once the knob rests (250ms).
//
// State, not events: the control task (Core 0) only records what the host *should* see --
// held mouse buttons, pending pointer travel, pivot requests -- and usb_task.c keeps sending
// until the host matches. A report lost to a busy endpoint therefore can't leave a button
// or modifier stuck down (the first hardware test did exactly that with queued edge events).
// Buttons are debounced here (raw GPIO bounce fired several F3 actions per press).

// Every control tick. `active` = APP mode selected and the menu closed; when false this just
// lets go of anything held and tracks the buttons, so no stale press fires on return.
// `held` is the UI_BTN_* mask; `swallow` = the screensaver is eating presses.
void app_mode_update(bool active, int64_t now_us, uint8_t held, bool swallow);

// Every control tick with the knob's shaft-angle change since the last tick (radians).
void app_mode_motion(float delta_rad, int64_t now_us);

// True while a drag (zoom/orbit/pan) is in progress. (APP mode sends no wheel steps, and
// runs the smooth VISCOSE feel throughout -- control_task.c.)
bool app_mode_dragging(void);

// --- USB task side ---
// What the host should see held: MOUSE_BUTTON_* mask, KEYBOARD_MODIFIER_* mask, and
// whether pointer travel goes on the y axis (zoom) instead of x (orbit/pan).
void app_mode_wanted(uint8_t *buttons, uint8_t *modifier, bool *axis_y);
int32_t app_mode_take_move_px(void);    // pointer travel accumulated since the last call
void app_mode_return_move_px(int32_t px); // give back what didn't fit in one report
bool app_mode_take_shortcut(uint8_t *modifier, uint8_t *keycode); // true once per F3 press
