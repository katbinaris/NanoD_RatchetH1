#include "app_mode.h"
#include "app_profiles/app_profiles.h"
#include "menu.h"
#include "ui_state.h"
#include <math.h>
#include <stdatomic.h>

#define APP_DRAG_START_PX 3.0f       // movement before a drag engages (ignores a wobble while
                                     // long-pressing F4 for the menu)
#define APP_MOTION_EPS_RAD 0.004f    // travel that counts as "the knob moved" (above sensor noise)
#define APP_IDLE_RELEASE_US 250000   // knob-alone slot: let go once the knob rests this long
#define APP_MENU_HOLD_US 700000      // long-press F4 -> menu
#define APP_DEBOUNCE_US 15000        // a button change counts once it has been stable this long
#define APP_TAP_RING 16              // queued key taps (a full ring drops taps, never releases)

static const uint8_t SLOT_KEY[APP_SLOT_COUNT] = {0, UI_BTN_F1, UI_BTN_F2, UI_BTN_F3, UI_BTN_F4};

// Control-task-only state.
static const app_profile_t *s_profile = NULL;
static bool s_active = false;
static uint8_t s_raw_held = 0, s_stable_held = 0, s_prev_held = 0;
static int64_t s_raw_since_us = 0;
static int s_slot = -1;             // live slot, -1 = none
static uint8_t s_slot_key = 0;      // UI_BTN_* that owns it (0 = knob alone)
static bool s_engaged = false;      // the slot has sent something (button/modifier down, a step)
static float s_accum_px = 0.0f;     // drag travel not yet published
static float s_motion_accum = 0.0f; // travel since the last "moved" mark
static int64_t s_slot_start_us = 0;
static int64_t s_last_motion_us = 0;

// Shared with the USB task. The held state is one packed word so buttons, modifier and axis
// always change together: bits 0-7 mouse buttons, 8-15 keyboard modifier, bit 16 = y axis.
static _Atomic uint32_t s_want = 0;
static _Atomic int32_t s_move_px = 0;
static _Atomic int32_t s_wheel_steps = 0;
static app_key_t s_taps[APP_TAP_RING];
static _Atomic uint32_t s_tap_head = 0, s_tap_tail = 0; // single producer (Core 0), single consumer (USB)
// Shared with the display task.
static _Atomic int s_live_slot = APP_SLOT_KNOB;

#define WANT(buttons, modifier, axis_y) ((uint32_t)(buttons) | ((uint32_t)(modifier) << 8) | ((axis_y) ? 1u << 16 : 0u))

static const app_action_t *action(int slot) {
    return &s_profile->slot[slot];
}

static void push_tap(app_key_t key) {
    uint32_t head = atomic_load(&s_tap_head);
    if (head - atomic_load(&s_tap_tail) >= APP_TAP_RING) return; // full: drop this tap
    s_taps[head % APP_TAP_RING] = key;
    atomic_store(&s_tap_head, head + 1);
}

static void end_slot(void) {
    atomic_store(&s_want, 0);
    atomic_store(&s_move_px, 0);
    atomic_store(&s_wheel_steps, 0);
    s_slot = -1;
    s_slot_key = 0;
    s_engaged = false;
    s_accum_px = 0.0f;
}

static void begin_slot(int slot, int64_t now_us) {
    if (s_slot >= 0) end_slot();
    s_slot = slot;
    s_slot_key = SLOT_KEY[slot];
    s_engaged = false;
    s_accum_px = 0.0f;
    s_slot_start_us = now_us;
    s_last_motion_us = now_us;
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

    const app_profile_t *p = app_profiles_get(menu_get_app_profile());
    if (p != s_profile) {
        end_slot();
        s_profile = p;
    }
    s_active = active && !swallow;
    if (!active) {
        if (s_slot >= 0 || atomic_load(&s_want)) end_slot();
        atomic_store(&s_live_slot, APP_SLOT_KNOB);
        return;
    }
    if (swallow) pressed = 0;

    for (int slot = APP_SLOT_F1; slot < APP_SLOT_COUNT; slot++) {
        if (!(pressed & SLOT_KEY[slot])) continue;
        const app_action_t *a = action(slot);
        if (a->kind == APP_ACT_TAP && slot != APP_SLOT_F4) {
            push_tap(a->cw);
        } else if (s_slot < 0 || s_slot_key == 0) {
            // A key takes over from the knob-alone slot. F4 always starts a slot (even with
            // no action) so its long press can open the menu.
            if (a->kind != APP_ACT_NONE || slot == APP_SLOT_F4) begin_slot(slot, now_us);
        }
    }
    if (s_slot_key != 0 && !(held & s_slot_key)) {
        end_slot();
    }
    if (s_slot == APP_SLOT_KNOB && now_us - s_last_motion_us >= APP_IDLE_RELEASE_US) {
        end_slot();
    }
    // F4 held with the knob still -> it was a long press for the menu.
    if (s_slot == APP_SLOT_F4 && !s_engaged && now_us - s_slot_start_us >= APP_MENU_HOLD_US) {
        end_slot();
        menu_input_toggle_open();
    }
    atomic_store(&s_live_slot, s_slot >= 0 ? s_slot : APP_SLOT_KNOB);
}

// The knob moved: make sure a slot is live (the knob-alone slot starts itself).
static bool knob_slot_ready(int64_t now_us) {
    if (!s_active || s_profile == NULL) return false;
    if (s_slot < 0) {
        if (s_stable_held != 0) return false; // a key is down but owns no turn action
        if (action(APP_SLOT_KNOB)->kind == APP_ACT_NONE) return false;
        begin_slot(APP_SLOT_KNOB, now_us);
    }
    return true;
}

void app_mode_motion(float delta_rad, int64_t now_us) {
    if (!s_active || s_profile == NULL) return;
    s_motion_accum += delta_rad;
    bool moved = fabsf(s_motion_accum) >= APP_MOTION_EPS_RAD;
    if (moved) s_motion_accum = 0.0f;
    if (s_slot < 0 && !moved) return; // sensor noise never starts the knob-alone slot
    if (!knob_slot_ready(now_us)) return;
    if (moved) s_last_motion_us = now_us;

    const app_action_t *a = action(s_slot);
    if (a->kind != APP_ACT_DRAG) return; // steps come from app_mode_detent()
    s_accum_px += delta_rad * a->px_per_rad * a->sign;
    if (!s_engaged) {
        if (fabsf(s_accum_px) < APP_DRAG_START_PX) return;
        s_engaged = true;
        atomic_store(&s_want, WANT(a->buttons, a->modifier, a->axis_y));
    }
    float px = truncf(s_accum_px);
    if (px != 0.0f) {
        s_accum_px -= px;
        atomic_fetch_add(&s_move_px, (int32_t)px);
    }
}

void app_mode_detent(int8_t dir, int64_t now_us) {
    if (!knob_slot_ready(now_us)) return;
    s_last_motion_us = now_us;
    const app_action_t *a = action(s_slot);
    switch (a->kind) {
        case APP_ACT_WHEEL:
            if (!s_engaged) {
                s_engaged = true;
                atomic_store(&s_want, WANT(0, a->modifier, false));
            }
            atomic_fetch_add(&s_wheel_steps, dir * a->sign);
            break;
        case APP_ACT_KEYS:
            s_engaged = true;
            push_tap(dir > 0 ? a->cw : a->ccw);
            break;
        default:
            break; // drags follow app_mode_motion(); taps fire on press
    }
}

void app_mode_haptics(haptic_type_t *type, uint32_t *detents) {
    if (s_profile == NULL) return;
    const app_action_t *a = action(s_slot >= 0 ? s_slot : APP_SLOT_KNOB);
    if (a->kind == APP_ACT_NONE || a->kind == APP_ACT_TAP) return;
    *type = a->feel;
    if (a->detents) *detents = a->detents;
}

int app_mode_live_slot(void) {
    return atomic_load(&s_live_slot);
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

int32_t app_mode_take_wheel_steps(void) {
    return atomic_exchange(&s_wheel_steps, 0);
}

void app_mode_return_wheel_steps(int32_t steps) {
    atomic_fetch_add(&s_wheel_steps, steps);
}

bool app_mode_take_tap(app_key_t *key) {
    uint32_t tail = atomic_load(&s_tap_tail);
    if (tail == atomic_load(&s_tap_head)) return false;
    *key = s_taps[tail % APP_TAP_RING];
    atomic_store(&s_tap_tail, tail + 1);
    return true;
}
