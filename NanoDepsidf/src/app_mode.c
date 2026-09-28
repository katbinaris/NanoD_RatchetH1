#include "app_mode.h"
#include "ipc.h"
#include "menu.h"
#include "ui_state.h"
#include "class/hid/hid.h"
#include <math.h>
#include <stdatomic.h>

// --- Plasticity profile (hardcoded for this test) ---
// Plasticity's default viewport navigation (doc.plasticity.xyz, "Operating the 3D
// viewport"): middle-drag orbits, right-drag pans, Ctrl + middle-drag zooms continuously.
// Zoom was the wheel at first -- one step per detent, a visible jump each time; the
// continuous drag follows the knob pixel by pixel instead. (macOS doesn't honor HID
// high-resolution wheel reports, so a finer wheel wasn't an option.)
// F3 was Alt + middle-click ("center the view on the cursor") -- dropped after the first
// test: it re-centers the view rather than setting an orbit pivot in place. It's now Undo,
// Cmd+Z for macOS (HID Left GUI = Cmd; use KEYBOARD_MODIFIER_LEFTCTRL on Windows).
#define APP_ORBIT_BUTTON MOUSE_BUTTON_MIDDLE
#define APP_PAN_BUTTON MOUSE_BUTTON_RIGHT
#define APP_ZOOM_BUTTON MOUSE_BUTTON_MIDDLE
#define APP_ZOOM_MODIFIER KEYBOARD_MODIFIER_LEFTCTRL
#define APP_F3_MODIFIER KEYBOARD_MODIFIER_LEFTGUI
#define APP_F3_KEYCODE HID_KEY_Z

#define APP_DRAG_PX_PER_RAD 120.0f  // ~750px of pointer travel per knob turn -- tune on hardware
#define APP_DRAG_SIGN 1             // flip if orbit/pan runs the wrong way for the knob direction
#define APP_ZOOM_SIGN (-1)          // zoom drags vertically; flip if zoom in/out is reversed
#define APP_DRAG_START_PX 3.0f      // movement before the drag button goes down (ignores a
                                    // wobble while long-pressing F4 for the menu)
#define APP_AUTO_ZOOM_IDLE_US 250000 // knob-alone zoom: let go once the knob rests this long
#define APP_MENU_HOLD_US 700000     // long-press F4 -> menu
#define APP_DEBOUNCE_US 15000       // a button change counts once it has been stable this long

// DRAG_ZOOM_AUTO: zoom with no key held -- started by the knob itself, ended by it resting.
typedef enum { DRAG_NONE, DRAG_ORBIT, DRAG_PAN, DRAG_ZOOM, DRAG_ZOOM_AUTO } drag_t;

// Control-task-only state.
static bool s_active = false;
static uint8_t s_raw_held = 0, s_stable_held = 0, s_prev_held = 0;
static int64_t s_raw_since_us = 0;
static drag_t s_drag = DRAG_NONE;
static uint8_t s_drag_key = 0;      // UI_BTN_* that owns the drag (0 for the auto zoom)
static bool s_engaged = false;      // drag's buttons/modifier requested
static float s_accum_px = 0.0f;     // travel not yet published (fraction, or before engaging)
static int64_t s_drag_start_us = 0;
static int64_t s_last_motion_us = 0;

// Shared with the USB task. The held state is one packed word so buttons, modifier and axis
// always change together: bits 0-7 mouse buttons, 8-15 keyboard modifier, bit 16 = y axis.
static _Atomic uint32_t s_want = 0;
static _Atomic int32_t s_move_px = 0;
static _Atomic uint32_t s_shortcut_requests = 0;

#define WANT(buttons, modifier, axis_y) ((uint32_t)(buttons) | ((uint32_t)(modifier) << 8) | ((axis_y) ? 1u << 16 : 0u))

static void end_drag(void) {
    atomic_store(&s_want, 0);
    atomic_store(&s_move_px, 0);
    s_drag = DRAG_NONE;
    s_drag_key = 0;
    s_engaged = false;
    s_accum_px = 0.0f;
}

static void begin_drag(drag_t d, uint8_t key, int64_t now_us) {
    if (s_drag != DRAG_NONE) end_drag();
    s_drag = d;
    s_drag_key = key;
    s_engaged = false;
    s_accum_px = 0.0f;
    s_drag_start_us = now_us;
    s_last_motion_us = now_us;
}

static bool is_zoom(drag_t d) {
    return d == DRAG_ZOOM || d == DRAG_ZOOM_AUTO;
}

void app_mode_update(bool active, int64_t now_us, uint8_t held, bool swallow) {
    if (held != s_raw_held) {
        s_raw_held = held;
        s_raw_since_us = now_us;
    }
    if (s_raw_held != s_stable_held && now_us - s_raw_since_us >= APP_DEBOUNCE_US) {
        s_stable_held = s_raw_held;
    }
    held = s_stable_held;
    uint8_t pressed = held & ~s_prev_held;
    s_prev_held = held;
    s_active = active && !swallow;
    if (!active) {
        if (s_drag != DRAG_NONE || atomic_load(&s_want)) end_drag();
        return;
    }
    if (swallow) pressed = 0;

    if (pressed & UI_BTN_F3) {
        atomic_fetch_add(&s_shortcut_requests, 1);
    }
    // A key press takes over from the knob-alone zoom.
    if (s_drag == DRAG_NONE || s_drag == DRAG_ZOOM_AUTO) {
        if (pressed & UI_BTN_F1) {
            begin_drag(DRAG_ZOOM, UI_BTN_F1, now_us);
        } else if (pressed & UI_BTN_F2) {
            begin_drag(DRAG_ORBIT, UI_BTN_F2, now_us);
        } else if (pressed & UI_BTN_F4) {
            begin_drag(DRAG_PAN, UI_BTN_F4, now_us);
        }
    }
    if (s_drag_key != 0 && !(held & s_drag_key)) {
        end_drag();
    }
    if (s_drag == DRAG_ZOOM_AUTO && now_us - s_last_motion_us >= APP_AUTO_ZOOM_IDLE_US) {
        end_drag();
    }
    // F4 held with the knob still -> it was a long press for the menu, not a pan.
    if (s_drag == DRAG_PAN && !s_engaged && now_us - s_drag_start_us >= APP_MENU_HOLD_US) {
        end_drag();
        menu_input_toggle_open();
    }
}

void app_mode_motion(float delta_rad, int64_t now_us) {
    if (!s_active) return;
    if (s_drag == DRAG_NONE) {
        if (s_stable_held != 0) return; // a key is down but not a drag key (F3)
        begin_drag(DRAG_ZOOM_AUTO, 0, now_us);
    }
    bool zoom = is_zoom(s_drag);
    s_accum_px += delta_rad * APP_DRAG_PX_PER_RAD * (zoom ? APP_ZOOM_SIGN : APP_DRAG_SIGN);
    if (delta_rad != 0.0f) s_last_motion_us = now_us;
    if (!s_engaged) {
        if (fabsf(s_accum_px) < APP_DRAG_START_PX) return;
        s_engaged = true;
        uint32_t want = zoom ? WANT(APP_ZOOM_BUTTON, APP_ZOOM_MODIFIER, true)
                      : s_drag == DRAG_ORBIT ? WANT(APP_ORBIT_BUTTON, 0, false)
                      : WANT(APP_PAN_BUTTON, 0, false);
        atomic_store(&s_want, want);
    }
    float px = truncf(s_accum_px);
    if (px != 0.0f) {
        s_accum_px -= px;
        atomic_fetch_add(&s_move_px, (int32_t)px);
    }
}

void app_mode_wanted(uint8_t *buttons, uint8_t *modifier, bool *axis_y) {
    uint32_t w = atomic_load(&s_want);
    *buttons = (uint8_t)w;
    *modifier = (uint8_t)(w >> 8);
    *axis_y = (w >> 16) & 1;
}

int32_t app_mode_take_move_px(void) {
    return atomic_exchange(&s_move_px, 0);
}

void app_mode_return_move_px(int32_t px) {
    atomic_fetch_add(&s_move_px, px);
}

bool app_mode_take_shortcut(uint8_t *modifier, uint8_t *keycode) {
    uint32_t n = atomic_load(&s_shortcut_requests);
    while (n > 0) {
        if (atomic_compare_exchange_weak(&s_shortcut_requests, &n, n - 1)) {
            *modifier = APP_F3_MODIFIER;
            *keycode = APP_F3_KEYCODE;
            return true;
        }
    }
    return false;
}

bool app_mode_dragging(void) {
    return s_drag != DRAG_NONE;
}
