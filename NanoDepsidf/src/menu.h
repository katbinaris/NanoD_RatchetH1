#pragma once

#include <stdint.h>
#include <stdbool.h>

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
#define MENU_VALUE_TEXT_LEN 12
#define MENU_TITLE_LEN 24

// One rendered row, label and value kept separate (not one concatenated string) so
// display_task.c can lay them out label-left/value-right, matching the UI preview mockup's
// justified row layout. `value` is "" for a submenu/action item (e.g. "Haptic Configurator",
// "Save") that has nothing to show on the right. Disabled items (MIDI Mapping when HID Type
// != MIDI) are never included here at all -- skipped during navigation -- so the renderer
// never needs a disabled/greyed style.
typedef struct {
    char label[MENU_LABEL_TEXT_LEN];
    char value[MENU_VALUE_TEXT_LEN];
    bool selected;
} menu_render_row_t;

typedef struct {
    bool open;    // false = menu closed entirely; display_task.c shows the Main Screen instead
    bool editing; // true = the selected row's value is being live-adjusted by knob rotation
    char title[MENU_TITLE_LEN]; // "" at the top-level screen (no title row there)
    int row_count;
    menu_render_row_t rows[MENU_MAX_VISIBLE_ITEMS];
} menu_render_snapshot_t;

void menu_init(void);

// Producer side (Core 0) -- call on each button's press edge / each detent-crossing tick.
void menu_input_toggle_open(void);        // F4: closed->open, or open at any depth->closed
void menu_input_back(void);               // F3: cancel edit, else back one level, else close
void menu_input_select(void);             // F1: enter submenu / enter edit / fire action / commit edit
void menu_input_rotate(int8_t direction); // knob tick, +1/-1: navigate list, or adjust value while editing

bool menu_is_open(void);

// Consumer side (Core 1). Thread-safe full-struct copy.
void menu_get_render_snapshot(menu_render_snapshot_t *out);

// TEMPORARY DIAGNOSTIC (DEVELOPMENT_PLAN.md Phase 8): the earlier "laggy roller" fixes
// (I2S/display priority equalization, roller anim-duration override, single-buffer/24-row
// revert) didn't resolve it, and I2S being fully disabled still didn't either -- rather than
// guess a sixth hypothesis, measure the actual Core0-input-to-Core1-render latency directly.
// esp_timer_get_time(), an atomic store -- cheap and non-blocking, safe to call from Core 0's
// real-time loop (unlike an ESP_LOGx call there, which is exactly what caused the Phase 2a
// watchdog incident this project already learned from). display_task.c reads this and logs
// the delta only on Core 1, only when it actually detects a change -- infrequent, safe.
int64_t menu_get_last_input_us(void);
