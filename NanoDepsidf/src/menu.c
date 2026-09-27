#include "menu.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "menu";

// --- Item/screen model ---

typedef enum {
    MENU_ITEM_SUBMENU, // enters a child screen
    MENU_ITEM_VALUE,   // enters edit mode; rendered as "label  value"
    MENU_ITEM_ACTION,  // fires immediately on select (e.g. Save), stays on the same screen
} menu_item_kind_t;

typedef struct menu_screen_s menu_screen_t;

typedef struct {
    const char *label;
    menu_item_kind_t kind;
    const menu_screen_t *submenu;                      // MENU_ITEM_SUBMENU
    void (*format_value)(char *buf, size_t buf_size);  // MENU_ITEM_VALUE
    void (*on_rotate)(int8_t direction);               // MENU_ITEM_VALUE, called while editing
    void (*on_action)(void);                           // MENU_ITEM_ACTION
    bool (*is_enabled)(void);                          // NULL = always enabled
} menu_item_t;

struct menu_screen_s {
    const char *title;
    const menu_item_t *items;
    int item_count;
};

// --- Placeholder settings state (step 1 only) ---
// Not persisted, not the real haptic/HID/boot config -- steps 2-7 (DEVELOPMENT_PLAN.md
// Phase 8) replace these with real NVS-backed values shared with control_task.c/usb_task.c/
// main.c.
//
// _Atomic, matching ui_state.h's cross-core convention: on_rotate() (called from
// menu_input_rotate() on Core 0) writes, format_value() (called from
// menu_get_render_snapshot() on Core 1) reads. Word-sized atomics are naturally
// torn-read-free and lock-free on this target -- no mutex needed for these scalars, and
// crucially nothing here can block Core 0's real-time loop (see the state-lock comment
// below for why that matters).

static _Atomic int32_t s_ph_detents = 24;
static _Atomic float s_ph_kp = 1.50f;
static _Atomic float s_ph_kd = 0.050f;

typedef enum { PH_HAPTIC_SAW, PH_HAPTIC_SINE, PH_HAPTIC_VISCOSE, PH_HAPTIC_TYPE_COUNT } ph_haptic_type_t;
static _Atomic ph_haptic_type_t s_ph_haptic_type = PH_HAPTIC_SAW;
static const char *ph_haptic_type_name(ph_haptic_type_t t) {
    switch (t) {
        case PH_HAPTIC_SAW: return "Saw";
        case PH_HAPTIC_SINE: return "Sine";
        case PH_HAPTIC_VISCOSE: return "Viscose";
        default: return "?";
    }
}

typedef enum { PH_SOUND_WOOD_TOCK, PH_SOUND_TICK_THUD, PH_SOUND_COUNT } ph_sound_t;
static _Atomic ph_sound_t s_ph_sound = PH_SOUND_WOOD_TOCK;
static const char *ph_sound_name(ph_sound_t s) {
    return (s == PH_SOUND_WOOD_TOCK) ? "Wood Tock" : "Tick Thud";
}

typedef enum { PH_HID_KEYBOARD, PH_HID_MOUSE, PH_HID_MIDI, PH_HID_TYPE_COUNT } ph_hid_type_t;
static _Atomic ph_hid_type_t s_ph_hid_type = PH_HID_MOUSE; // matches today's real default (mouse-wheel mapping)
static const char *ph_hid_type_name(ph_hid_type_t t) {
    switch (t) {
        case PH_HID_KEYBOARD: return "Keyboard";
        case PH_HID_MOUSE: return "Mouse";
        case PH_HID_MIDI: return "MIDI";
        default: return "?";
    }
}

static _Atomic int32_t s_ph_midi_channel = 1; // 1-16

typedef enum { PH_BOOT_SERIAL, PH_BOOT_HID, PH_BOOT_MODE_COUNT } ph_boot_mode_t;
static _Atomic ph_boot_mode_t s_ph_boot_mode = PH_BOOT_HID; // matches today's real default (normal boot = composite HID+CDC)
static const char *ph_boot_mode_name(ph_boot_mode_t m) {
    return (m == PH_BOOT_SERIAL) ? "Serial" : "HID";
}

// --- Field callbacks ---
// on_rotate() runs on Core 0 (called from menu_input_rotate()); format_value() runs on
// Core 1 (called from menu_get_render_snapshot()). Both sides only ever touch the atomics
// above -- no shared mutable buffers, so there's nothing here for a lock to protect.

static void fmt_detents(char *buf, size_t n) {
    snprintf(buf, n, "%ld", (long)atomic_load_explicit(&s_ph_detents, memory_order_relaxed));
}
static void rotate_detents(int8_t dir) {
    int32_t v = atomic_load_explicit(&s_ph_detents, memory_order_relaxed) + dir;
    if (v < 0) v = 0;
    if (v > 120) v = 120;
    atomic_store_explicit(&s_ph_detents, v, memory_order_relaxed);
}

static void fmt_kp(char *buf, size_t n) {
    snprintf(buf, n, "%.2f", (double)atomic_load_explicit(&s_ph_kp, memory_order_relaxed));
}
static void rotate_kp(int8_t dir) {
    float v = atomic_load_explicit(&s_ph_kp, memory_order_relaxed) + dir * 0.05f;
    if (v < 0.0f) v = 0.0f;
    if (v > 20.0f) v = 20.0f;
    atomic_store_explicit(&s_ph_kp, v, memory_order_relaxed);
}

static void fmt_kd(char *buf, size_t n) {
    snprintf(buf, n, "%.3f", (double)atomic_load_explicit(&s_ph_kd, memory_order_relaxed));
}
static void rotate_kd(int8_t dir) {
    float v = atomic_load_explicit(&s_ph_kd, memory_order_relaxed) + dir * 0.005f;
    if (v < 0.0f) v = 0.0f;
    if (v > 0.15f) v = 0.15f;
    atomic_store_explicit(&s_ph_kd, v, memory_order_relaxed);
}

static void fmt_haptic_type(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_haptic_type_name(atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed)));
}
static void rotate_haptic_type(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed) + dir) % PH_HAPTIC_TYPE_COUNT;
    if (v < 0) v += PH_HAPTIC_TYPE_COUNT;
    atomic_store_explicit(&s_ph_haptic_type, (ph_haptic_type_t)v, memory_order_relaxed);
}

static void fmt_sound(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_sound_name(atomic_load_explicit(&s_ph_sound, memory_order_relaxed)));
}
static void rotate_sound(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_sound, memory_order_relaxed) + dir) % PH_SOUND_COUNT;
    if (v < 0) v += PH_SOUND_COUNT;
    atomic_store_explicit(&s_ph_sound, (ph_sound_t)v, memory_order_relaxed);
}

static void action_save_haptic(void) {
    ESP_LOGI(TAG, "Save (Haptic Configurator) -- placeholder, NVS not wired yet (Phase 8 step 2)");
}

static void fmt_hid_type(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_hid_type_name(atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed)));
}
static void rotate_hid_type(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed) + dir) % PH_HID_TYPE_COUNT;
    if (v < 0) v += PH_HID_TYPE_COUNT;
    atomic_store_explicit(&s_ph_hid_type, (ph_hid_type_t)v, memory_order_relaxed);
}
static bool midi_mapping_enabled(void) {
    return atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed) == PH_HID_MIDI;
}

static void fmt_midi_mapping(char *buf, size_t n) {
    snprintf(buf, n, "Ch %ld", (long)atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed));
}
static void rotate_midi_mapping(int8_t dir) {
    int32_t v = atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed) + dir;
    if (v < 1) v = 1;
    if (v > 16) v = 16;
    atomic_store_explicit(&s_ph_midi_channel, v, memory_order_relaxed);
}

static void action_save_hid(void) {
    ESP_LOGI(TAG, "Save (HID Type) -- placeholder, NVS not wired yet (Phase 8 step 2/7)");
}

static void fmt_boot_mode(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_boot_mode_name(atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed)));
}
static void rotate_boot_mode(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed) + dir) % PH_BOOT_MODE_COUNT;
    if (v < 0) v += PH_BOOT_MODE_COUNT;
    atomic_store_explicit(&s_ph_boot_mode, (ph_boot_mode_t)v, memory_order_relaxed);
}
static void action_save_boot(void) {
    ESP_LOGI(TAG, "Save (Boot USB Mode) -- placeholder, NVS not wired yet (Phase 8 step 2/6)");
}

// --- Screens ---

static const menu_item_t s_haptic_items[] = {
    { .label = "Detents",      .kind = MENU_ITEM_VALUE,  .format_value = fmt_detents,      .on_rotate = rotate_detents },
    { .label = "Kp",           .kind = MENU_ITEM_VALUE,  .format_value = fmt_kp,           .on_rotate = rotate_kp },
    { .label = "Kd",           .kind = MENU_ITEM_VALUE,  .format_value = fmt_kd,           .on_rotate = rotate_kd },
    { .label = "Haptic Type",  .kind = MENU_ITEM_VALUE,  .format_value = fmt_haptic_type,  .on_rotate = rotate_haptic_type },
    { .label = "Haptic Sound", .kind = MENU_ITEM_VALUE,  .format_value = fmt_sound,        .on_rotate = rotate_sound },
    { .label = "Save",         .kind = MENU_ITEM_ACTION, .on_action = action_save_haptic },
};
static const menu_screen_t s_haptic_screen = {
    "Haptic Configurator", s_haptic_items, sizeof(s_haptic_items) / sizeof(s_haptic_items[0])
};

static const menu_item_t s_hid_items[] = {
    { .label = "HID Type",     .kind = MENU_ITEM_VALUE,  .format_value = fmt_hid_type,     .on_rotate = rotate_hid_type },
    { .label = "MIDI Mapping", .kind = MENU_ITEM_VALUE,  .format_value = fmt_midi_mapping, .on_rotate = rotate_midi_mapping, .is_enabled = midi_mapping_enabled },
    { .label = "Save",         .kind = MENU_ITEM_ACTION, .on_action = action_save_hid },
};
static const menu_screen_t s_hid_screen = {
    "HID Type", s_hid_items, sizeof(s_hid_items) / sizeof(s_hid_items[0])
};

static const menu_item_t s_boot_items[] = {
    { .label = "USB Mode", .kind = MENU_ITEM_VALUE,  .format_value = fmt_boot_mode, .on_rotate = rotate_boot_mode },
    { .label = "Save",     .kind = MENU_ITEM_ACTION, .on_action = action_save_boot },
};
static const menu_screen_t s_boot_screen = {
    "Boot USB Mode", s_boot_items, sizeof(s_boot_items) / sizeof(s_boot_items[0])
};

static const menu_item_t s_root_items[] = {
    { .label = "Haptic Configurator", .kind = MENU_ITEM_SUBMENU, .submenu = &s_haptic_screen },
    { .label = "HID Type",            .kind = MENU_ITEM_SUBMENU, .submenu = &s_hid_screen },
    { .label = "Boot USB Mode",       .kind = MENU_ITEM_SUBMENU, .submenu = &s_boot_screen },
};
static const menu_screen_t s_root_screen = {
    "", s_root_items, sizeof(s_root_items) / sizeof(s_root_items[0])
};

// --- Navigation state ---
// A stack rather than separate "MenuRoot"/"Section" state: depth 0 = closed, depth 1 =
// top-level screen, depth 2+ = however deep a submenu goes. F3 (back) pops one frame; F1
// (select) on a MENU_ITEM_SUBMENU pushes one.
//
// **Real-time note, learned the hard way on hardware**: menu_input_*() below runs on Core 0
// inside control_task.c's hard-real-time 10kHz loop (one call per button edge / per detent
// crossing). The first version of this file recomputed a fully-formatted render snapshot
// (snprintf across up to 8 rows) inline on every such call, guarded by a FreeRTOS mutex
// shared with the display task. At slow rotation that's rarely a problem, but fast CW
// rotation drives many detent crossings per second -- each one doing variable-cost
// string-formatting work (and a mutex take that can block if Core 1 happens to hold it)
// inside a loop whose slew-rate-limiter math assumes a fixed 100us dt per iteration.
// Repeated budget overruns clustering at high spin rates manifested on hardware as the
// knob "overshooting and spinning by itself" -- the same class of scheduler-starvation bug
// as Phase 2a's per-tick-logging watchdog incident, just triggered by string formatting
// instead of UART logging. Fixed by only ever touching a few bounded ints/pointers here,
// under a spinlock (portMUX, non-blocking, unlike a mutex) -- all string formatting moved
// to menu_get_render_snapshot(), called only from Core 1's relaxed ~30ms display poll,
// which has no real-time constraint to violate.

#define MENU_MAX_DEPTH 4

typedef struct {
    const menu_screen_t *screen;
    int selected_index;
} menu_stack_frame_t;

static menu_stack_frame_t s_stack[MENU_MAX_DEPTH];
static int s_stack_depth = 0; // 0 = closed
static bool s_editing = false;
static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;

static bool item_enabled(const menu_screen_t *screen, int index) {
    const menu_item_t *it = &screen->items[index];
    return it->is_enabled == NULL || it->is_enabled();
}

static int first_enabled_index(const menu_screen_t *screen) {
    for (int i = 0; i < screen->item_count; i++) {
        if (item_enabled(screen, i)) return i;
    }
    return -1; // no enabled items -- shouldn't happen with any screen defined above
}

// Steps the selection by +-1, skipping disabled items, wrapping at both ends. Bounded by
// item_count iterations (at most 6 today) so this stays cheap and deterministic even
// though it's called from Core 0's real-time loop.
static int step_index(const menu_screen_t *screen, int current, int8_t dir) {
    if (screen->item_count <= 0) return current;
    int idx = current;
    for (int i = 0; i < screen->item_count; i++) {
        idx = (idx + dir) % screen->item_count;
        if (idx < 0) idx += screen->item_count;
        if (item_enabled(screen, idx)) return idx;
    }
    return current;
}

void menu_init(void) {
    portENTER_CRITICAL(&s_state_mux);
    s_stack_depth = 0;
    s_editing = false;
    portEXIT_CRITICAL(&s_state_mux);
}

// TEMPORARY DIAGNOSTIC -- see menu.h's menu_get_last_input_us() comment.
static _Atomic int64_t s_last_input_us = 0;
static inline void stamp_input_time(void) {
    atomic_store_explicit(&s_last_input_us, esp_timer_get_time(), memory_order_relaxed);
}

void menu_input_toggle_open(void) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0) {
        int idx = first_enabled_index(&s_root_screen);
        s_stack[0].screen = &s_root_screen;
        s_stack[0].selected_index = (idx < 0) ? 0 : idx;
        s_stack_depth = 1;
    } else {
        s_stack_depth = 0;
        s_editing = false;
    }
    portEXIT_CRITICAL(&s_state_mux);
    stamp_input_time();
}

void menu_input_back(void) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0) {
        // no-op
    } else if (s_editing) {
        // Cancel. Step 1's placeholder items apply changes immediately via on_rotate with
        // no snapshot taken on edit-enter, so there's nothing to revert yet -- a real
        // revert-on-cancel (on_edit_enter/on_edit_cancel hooks) is a natural addition once
        // step 3 wires in fields that actually need it.
        s_editing = false;
    } else if (s_stack_depth > 1) {
        s_stack_depth--;
    } else {
        s_stack_depth = 0; // at the top-level screen -- back behaves like close
    }
    portEXIT_CRITICAL(&s_state_mux);
    stamp_input_time();
}

void menu_input_select(void) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0) {
        portEXIT_CRITICAL(&s_state_mux);
        stamp_input_time();
        return;
    }
    menu_stack_frame_t *top = &s_stack[s_stack_depth - 1];
    const menu_item_t *it = &top->screen->items[top->selected_index];

    if (s_editing) {
        s_editing = false; // commit -- see menu_input_back()'s comment on why this is a no-op today
    } else {
        switch (it->kind) {
            case MENU_ITEM_SUBMENU:
                if (s_stack_depth < MENU_MAX_DEPTH && it->submenu != NULL) {
                    int idx = first_enabled_index(it->submenu);
                    s_stack[s_stack_depth].screen = it->submenu;
                    s_stack[s_stack_depth].selected_index = (idx < 0) ? 0 : idx;
                    s_stack_depth++;
                }
                break;
            case MENU_ITEM_VALUE:
                s_editing = true;
                break;
            case MENU_ITEM_ACTION:
                // on_action() (e.g. a Save placeholder's ESP_LOGI) runs here, still inside
                // the critical section -- fine for now since these are trivial no-ops, but
                // worth remembering once step 2/6/7 wire in real NVS writes: an NVS commit
                // is NOT bounded/cheap and must not run under this spinlock. Move it outside
                // (copy out the "save requested" intent, act on it after portEXIT_CRITICAL)
                // when that lands.
                if (it->on_action != NULL) {
                    it->on_action();
                }
                break;
        }
    }
    portEXIT_CRITICAL(&s_state_mux);
    stamp_input_time();
}

void menu_input_rotate(int8_t direction) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0 || direction == 0) {
        portEXIT_CRITICAL(&s_state_mux);
        stamp_input_time();
        return;
    }
    menu_stack_frame_t *top = &s_stack[s_stack_depth - 1];
    if (s_editing) {
        const menu_item_t *it = &top->screen->items[top->selected_index];
        if (it->on_rotate != NULL) {
            it->on_rotate(direction); // atomic store(s) only -- see field callbacks above
        }
    } else {
        top->selected_index = step_index(top->screen, top->selected_index, direction);
    }
    portEXIT_CRITICAL(&s_state_mux);
    stamp_input_time();
}

int64_t menu_get_last_input_us(void) {
    return atomic_load_explicit(&s_last_input_us, memory_order_relaxed);
}

bool menu_is_open(void) {
    portENTER_CRITICAL(&s_state_mux);
    bool open = (s_stack_depth > 0);
    portEXIT_CRITICAL(&s_state_mux);
    return open;
}

// Core 1 only. Takes a quick, bounded copy of the raw navigation state under the same
// spinlock Core 0 uses (held only long enough to copy a few words), then does all string
// formatting outside any lock -- safe to be as slow as it likes here, this task has no
// real-time deadline to violate.
void menu_get_render_snapshot(menu_render_snapshot_t *out) {
    menu_stack_frame_t top_copy;
    int depth_copy;
    bool editing_copy;

    portENTER_CRITICAL(&s_state_mux);
    depth_copy = s_stack_depth;
    editing_copy = s_editing;
    if (depth_copy > 0) {
        top_copy = s_stack[depth_copy - 1];
    }
    portEXIT_CRITICAL(&s_state_mux);

    menu_render_snapshot_t snap = { 0 };
    snap.open = (depth_copy > 0);
    snap.editing = editing_copy;

    if (snap.open) {
        const menu_screen_t *screen = top_copy.screen;
        snprintf(snap.title, sizeof(snap.title), "%s", screen->title);

        int row = 0;
        for (int i = 0; i < screen->item_count && row < MENU_MAX_VISIBLE_ITEMS; i++) {
            if (!item_enabled(screen, i)) {
                continue; // disabled items are skipped entirely, not shown greyed-out
            }
            const menu_item_t *it = &screen->items[i];
            menu_render_row_t *r = &snap.rows[row];
            snprintf(r->label, sizeof(r->label), "%s", it->label);
            if (it->kind == MENU_ITEM_VALUE && it->format_value != NULL) {
                it->format_value(r->value, sizeof(r->value));
            } else {
                r->value[0] = '\0'; // submenu/action rows have nothing to show on the right
            }
            r->selected = (i == top_copy.selected_index);
            row++;
        }
        snap.row_count = row;
    }

    *out = snap;
}
