#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "haptic_params.h"
#include "audio_trigger.h"
#include "boot_mode.h"

// Phase 8: real configuration menu, replacing the Phase 4 mock (three static labels,
// BTN_D toggle only). See DEVELOPMENT_PLAN.md Phase 8 for the full screen hierarchy,
// button roles and 8-step build order -- this file is step 1: the navigation framework +
// data-driven rendering. Placeholder settings values only (menu.c) -- no real haptic/HID/
// boot config is read or written yet, that's steps 2-7.
//
// Owned/driven from Core 0 (control_task.c, where button/knob reads already live per the
// architecture log in DEVELOPMENT_PLAN.md) -- menu_input_*() below must stay cheap and
// bounded, since they run inside that file's hard-real-time 10kHz control loop. All string
// formatting happens lazily on the consumer side instead (menu_get_render_snapshot(),
// called only from display_task.c on Core 1, which has no real-time deadline to violate) --
// see the state-lock comment in menu.c for the hardware incident that made this the design,
// not a stylistic choice.

#define MENU_MAX_VISIBLE_ITEMS 8
#define MENU_LABEL_TEXT_LEN 20
#define MENU_CAPTION_TEXT_LEN 12
#define MENU_VALUE_TEXT_LEN 12
#define MENU_TITLE_LEN 24

// Pixel UI (DEVELOPMENT_PLAN.md "Pixel UI"): every screen has its own layout on the display
// side (text list, Orbit dashboard, HID carousel, Boot mode cards), so the snapshot names the
// screen explicitly instead of the renderer guessing from the title.
typedef enum {
    MENU_SCREEN_NONE = 0, // menu closed -- Main Screen
    MENU_SCREEN_ROOT,
    MENU_SCREEN_HAPTIC,
    MENU_SCREEN_HID,
    MENU_SCREEN_BOOT,
} menu_screen_id_t;

// Row indices of the Haptic screen -- display_task.cpp keys its per-setting icons and the
// FEEL animation off these.
enum {
    MENU_HAPTIC_ROW_STEPS = 0,
    MENU_HAPTIC_ROW_SNAP,
    MENU_HAPTIC_ROW_DAMP,
    MENU_HAPTIC_ROW_FEEL,
    MENU_HAPTIC_ROW_TONE,
    MENU_HAPTIC_ROW_PITCH,
    MENU_HAPTIC_ROW_COUNT,
};

// APP: an application profile (for now one hardcoded Plasticity profile, control_task.c) --
// F1-F4 become app controls (zoom/orbit/pivot/pan) and long-press F4 opens the menu.
typedef enum { MENU_HID_KEYBOARD = 0, MENU_HID_MOUSE, MENU_HID_MIDI, MENU_HID_APP, MENU_HID_TYPE_COUNT } menu_hid_type_t;

// One rendered row. `caption` is the small engineering name shown under the friendly label
// (e.g. label "SNAP", caption "KP"); "" where there is none. `value` is "" for a submenu item
// that has nothing to show. Disabled items (MIDI channel when HID type != MIDI) are never
// included here at all -- skipped during navigation -- so the renderer never needs a
// disabled/greyed style.
typedef struct {
    char label[MENU_LABEL_TEXT_LEN];
    char caption[MENU_CAPTION_TEXT_LEN];
    char value[MENU_VALUE_TEXT_LEN];
    bool selected;
} menu_render_row_t;

typedef struct {
    bool open;    // false = menu closed entirely; display_task.cpp shows the Main Screen instead
    bool editing; // true = the selected row's value is being live-adjusted by knob rotation
    bool dirty;   // the current screen has changes F2 hasn't saved yet
    menu_screen_id_t screen;
    int selected;        // index into rows[] of the selected row, -1 if none
    uint32_t save_count; // bumps on every successful F2 save -- the display's SAVED! cue
    char title[MENU_TITLE_LEN]; // "" at the top-level screen (no title row there)
    int row_count;
    menu_render_row_t rows[MENU_MAX_VISIBLE_ITEMS];
} menu_render_snapshot_t;

void menu_init(void);

// Producer side (Core 0) -- call on each button's press edge / each detent-crossing tick.
//
// Two kinds of settings screen:
//   - Haptic (Orbit): turn moves focus; F1 enters edit, turn changes the value live, F1
//     confirms; F3 cancels the edit and restores the value from before it. Unsaved changes
//     stay live after leaving the screen (tune by feel, save when it's right).
//   - HID / Boot mode ("direct"): turn changes the focused value immediately; F1 moves focus
//     to the next field (HID type <-> MIDI channel). Leaving with F3 or F4 discards unsaved
//     changes -- these aren't live-tunable, only a saved choice means anything.
void menu_input_toggle_open(void);        // F4: closed->open, or open at any depth->closed
void menu_input_back(void);               // F3: cancel edit, else back one level, else close
void menu_input_select(void);             // F1: enter submenu / enter or confirm edit / next field
void menu_input_save(void);               // F2: save the current settings screen (if changed)
void menu_input_rotate(int8_t direction); // knob tick, +1/-1: navigate list, or adjust value while editing

bool menu_is_open(void);

// Consumer side (Core 1). Thread-safe full-struct copy.
void menu_get_render_snapshot(menu_render_snapshot_t *out);

// Phase 8 step 3: live haptic settings, adjustable via the Haptic Configurator screen and
// read directly by control_task.c's real-time haptic loop -- replaces the retired
// button-combo live-tuning path. Safe to call from Core 0's real-time loop: each is a single
// atomic load, no lock, same convention as the rest of this file. num_detents is always in
// [HAPTIC_NUM_DETENTS_MIN, HAPTIC_NUM_DETENTS_MAX] (haptic_params.h) -- never 0.
uint32_t menu_get_haptic_num_detents(void);
float menu_get_haptic_kp(void);
float menu_get_haptic_kd(void);
haptic_type_t menu_get_haptic_type(void);

// Phase 8 step 5: live click timbre, adjustable via the Haptic Configurator's "Haptic Sound"
// field and read directly by i2s_task.c (Core 1) when a new detent click starts. Same
// lock-free atomic-load convention as the getters above, just consumed by a different task.
audio_click_timbre_t menu_get_haptic_sound(void);
float menu_get_haptic_pitch(void);

// Phase 8 step 6: live boot USB mode, read once by main.c at startup (before any task
// starts, so no cross-core-timing concern) -- loaded from NVS in menu_init() same as
// everything else above, combined there with the BTN_C+BTN_D hold failsafe (which always
// takes priority regardless of this saved setting).
boot_usb_mode_t menu_get_boot_mode(void);

// Pixel UI: read by display_task.cpp for the Main Screen mode icon and the HID carousel.
menu_hid_type_t menu_get_hid_type(void);

// TEMPORARY DIAGNOSTIC (DEVELOPMENT_PLAN.md Phase 8): the earlier "laggy roller" fixes
// (I2S/display priority equalization, roller anim-duration override, single-buffer/24-row
// revert) didn't resolve it, and I2S being fully disabled still didn't either -- rather than
// guess a sixth hypothesis, measure the actual Core0-input-to-Core1-render latency directly.
// esp_timer_get_time(), an atomic store -- cheap and non-blocking, safe to call from Core 0's
// real-time loop (unlike an ESP_LOGx call there, which is exactly what caused the Phase 2a
// watchdog incident this project already learned from). display_task.c reads this and logs
// the delta only on Core 1, only when it actually detects a change -- infrequent, safe.
int64_t menu_get_last_input_us(void);
