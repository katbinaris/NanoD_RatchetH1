#include "app_mode.h"
#include "app_profiles/app_profiles.h"
#include "menu.h"
#include "ui_state.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include "class/hid/hid.h"

#define APP_DRAG_START_PX 3.0f       // movement before a drag engages (ignores a wobble while
                                     // long-pressing F4 for the menu)
#define APP_MOTION_EPS_RAD 0.004f    // travel that counts as "the knob moved" (above sensor noise)
#define APP_IDLE_RELEASE_US 250000   // knob-alone slot: let go once the knob rests this long
#define APP_MENU_HOLD_US 700000      // long-press F4 -> menu
#define APP_DEBOUNCE_US 15000        // a button change counts once it has been stable this long
#define APP_TAP_RING 64              // queued key taps (a full ring drops taps, never releases);
                                     // sized for one command-search macro (open + ~40 chars + Enter)
#define APP_TAP_MAX_US 400000        // a key let go within this, without turning, was a tap
#define APP_CANCEL 0                 // wheel entry 0 of every ring: close without running

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
static app_tap_t s_taps[APP_TAP_RING];
static _Atomic uint32_t s_tap_head = 0, s_tap_tail = 0; // single producer (Core 0), single consumer (USB)
// Shared with the display task.
static _Atomic int s_live_slot = APP_SLOT_KNOB;
// Command wheel: bit 31 open, bits 8-15 ring, 0-7 entry (0 = cancel). Runs: a counter plus
// which command, so the display can echo it.
static _Atomic uint32_t s_wheel = 0;
static _Atomic uint32_t s_run = 0; // bits 16-31 count, 8-15 ring, 0-7 entry

// Command wheel, control-task side.
static bool s_wheel_open = false;
static uint8_t s_ring = 0, s_entry = 1;
static uint8_t s_ring_entry[8];     // last entry used per ring (opens there: hold-release repeats)
static uint8_t s_swallow_keys = 0;  // F keys used to switch rings: their release does nothing

#define WANT(buttons, modifier, axis_y) ((uint32_t)(buttons) | ((uint32_t)(modifier) << 8) | ((axis_y) ? 1u << 16 : 0u))

static const app_action_t *action(int slot) {
    return &s_profile->slot[slot];
}

static uint32_t tap_room(void) {
    return APP_TAP_RING - (atomic_load(&s_tap_head) - atomic_load(&s_tap_tail));
}

static void push_tap_wait(app_key_t key, uint8_t wait_ticks) {
    uint32_t head = atomic_load(&s_tap_head);
    if (head - atomic_load(&s_tap_tail) >= APP_TAP_RING) return; // full: drop this tap
    s_taps[head % APP_TAP_RING] = (app_tap_t){key.modifier, key.keycode, wait_ticks};
    atomic_store(&s_tap_head, head + 1);
}

static void push_tap(app_key_t key) {
    push_tap_wait(key, 0);
}

// US key positions for the characters a command-search phrase may use.
static bool ascii_key(char c, app_key_t *k) {
    k->modifier = 0;
    if (c >= 'a' && c <= 'z') k->keycode = HID_KEY_A + (c - 'a');
    else if (c >= 'A' && c <= 'Z') { k->keycode = HID_KEY_A + (c - 'A'); k->modifier = KEYBOARD_MODIFIER_LEFTSHIFT; }
    else if (c >= '1' && c <= '9') k->keycode = HID_KEY_1 + (c - '1');
    else if (c == '0') k->keycode = HID_KEY_0;
    else if (c == ' ') k->keycode = HID_KEY_SPACE;
    else if (c == '-') k->keycode = HID_KEY_MINUS;
    else if (c == '.') k->keycode = HID_KEY_PERIOD;
    else return false;
    return true;
}

// ACTIONS: open the app's command search, type the phrase, wait for the results, Enter. All
// or nothing -- a macro cut short by a full ring would type half a phrase.
static void push_search(const app_search_t *s, const char *phrase) {
    uint32_t need = 2 + (uint32_t)strlen(phrase);
    if (tap_room() < need) return;
    push_tap_wait(s->open, s->open_wait);
    for (const char *c = phrase; *c; c++) {
        app_key_t k;
        if (ascii_key(*c, &k)) push_tap_wait(k, c[1] ? 0 : s->result_wait);
    }
    push_tap((app_key_t){0, HID_KEY_ENTER});
}

static int ring_count(void) {
    int n = s_profile->ring_count;
    return n > (int)sizeof(s_ring_entry) ? (int)sizeof(s_ring_entry) : n;
}

static void publish_wheel(void) {
    atomic_store(&s_wheel, (s_wheel_open ? 1u << 31 : 0u) | ((uint32_t)s_ring << 8) | s_entry);
}

static void wheel_open(void) {
    if (ring_count() == 0) return;
    if (s_ring >= ring_count()) s_ring = 0;
    s_entry = s_ring_entry[s_ring] ? s_ring_entry[s_ring] : 1;
    s_wheel_open = true;
    publish_wheel();
}

// F3 let go: run the chosen command (entry 0 = cancel).
static void wheel_close(void) {
    if (!s_wheel_open) return;
    s_wheel_open = false;
    const app_ring_t *r = &s_profile->rings[s_ring];
    if (s_entry != APP_CANCEL && s_entry <= r->count) {
        const app_cmd_t *c = &r->cmds[s_entry - 1];
        if (c->kind == APP_CMD_ACTIONS && c->phrase) push_search(&s_profile->search, c->phrase);
        else push_tap(c->key);
        s_ring_entry[s_ring] = s_entry;
        uint32_t n = (atomic_load(&s_run) >> 16) + 1;
        atomic_store(&s_run, (n << 16) | ((uint32_t)s_ring << 8) | s_entry);
    }
    publish_wheel();
}

static void end_slot(void) {
    s_wheel_open = false;
    publish_wheel();
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
    s_swallow_keys &= held; // forget keys once they're up
    if (!active) {
        if (s_slot >= 0 || atomic_load(&s_want)) end_slot();
        atomic_store(&s_live_slot, APP_SLOT_KNOB);
        return;
    }
    if (swallow) pressed = 0;

    // Command wheel open: the other F keys jump between rings and do nothing else.
    if (s_wheel_open) {
        for (int i = 0; i < ring_count(); i++) {
            uint8_t k = SLOT_KEY[s_profile->rings[i].slot];
            if ((pressed & k) && k != s_slot_key) {
                s_ring = (uint8_t)i;
                s_entry = s_ring_entry[i] ? s_ring_entry[i] : 1;
                publish_wheel();
            }
        }
        s_swallow_keys |= pressed & ~s_slot_key;
        pressed &= s_slot_key;
    }
    pressed &= ~s_swallow_keys;

    for (int slot = APP_SLOT_F1; slot < APP_SLOT_COUNT; slot++) {
        if (!(pressed & SLOT_KEY[slot])) continue;
        const app_action_t *a = action(slot);
        if (a->kind == APP_ACT_TAP && slot != APP_SLOT_F4) {
            push_tap(a->cw);
        } else if (s_slot < 0 || s_slot_key == 0) {
            // A key takes over from the knob-alone slot. F4 always starts a slot (even with
            // no action) so its long press can open the menu.
            if (a->kind != APP_ACT_NONE || slot == APP_SLOT_F4) begin_slot(slot, now_us);
            if (a->kind == APP_ACT_COMMANDS && slot != APP_SLOT_F4) wheel_open();
        }
    }
    if (s_slot_key != 0 && !(held & s_slot_key)) {
        // Let go: the wheel runs its command; a quick press that never turned is a tap.
        const app_action_t *a = action(s_slot);
        if (s_wheel_open) wheel_close();
        else if (a->tap.keycode && !s_engaged && now_us - s_slot_start_us < APP_TAP_MAX_US) push_tap(a->tap);
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
        case APP_ACT_COMMANDS: {
            // One detent per entry; the ends just stop (cancel first, last command last).
            if (!s_wheel_open) break;
            int last = s_profile->rings[s_ring].count;
            int e = s_entry + dir;
            if (e >= 0 && e <= last) {
                s_entry = (uint8_t)e;
                publish_wheel();
            }
            s_engaged = true;
            break;
        }
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

bool app_mode_wheel(int *ring, int *entry) {
    uint32_t w = atomic_load(&s_wheel);
    *ring = (w >> 8) & 0xFF;
    *entry = w & 0xFF;
    return (w >> 31) != 0;
}

uint32_t app_mode_last_run(int *ring, int *entry) {
    uint32_t r = atomic_load(&s_run);
    *ring = (r >> 8) & 0xFF;
    *entry = r & 0xFF;
    return r >> 16;
}

bool app_mode_take_tap(app_tap_t *key) {
    uint32_t tail = atomic_load(&s_tap_tail);
    if (tail == atomic_load(&s_tap_head)) return false;
    *key = s_taps[tail % APP_TAP_RING];
    atomic_store(&s_tap_tail, tail + 1);
    return true;
}
