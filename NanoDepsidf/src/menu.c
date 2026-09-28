#include "menu.h"
#include "config_store.h"
#include "haptic_params.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

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

// Phase 8 step 3: these three are no longer step-1 placeholders -- control_task.c's
// real-time haptic loop reads them directly (menu_get_haptic_*() below), so their
// defaults/bounds now come from haptic_params.h (shared with control_task.c) instead of
// independent literals. In particular num_detents must never reach 0 -- it divides directly
// into 2*pi in that loop -- so its clamp uses HAPTIC_NUM_DETENTS_MIN/MAX, not the old
// unconnected 0-120 placeholder range.
static _Atomic int32_t s_ph_detents = HAPTIC_NUM_DETENTS_DEFAULT;
static _Atomic float s_ph_kp = HAPTIC_KP_DEFAULT;
static _Atomic float s_ph_kd = HAPTIC_KD_DEFAULT;

// haptic_type_t itself now lives in haptic_params.h (Phase 8 step 4) -- control_task.c
// branches on it directly to pick a restoring-force law, so it can no longer be a
// menu.c-private enum the way it was in steps 1-3.
static _Atomic haptic_type_t s_ph_haptic_type = HAPTIC_TYPE_SAW;
static const char *ph_haptic_type_name(haptic_type_t t) {
    switch (t) {
        case HAPTIC_TYPE_SAW: return "Saw";
        case HAPTIC_TYPE_SINE: return "Sine";
        case HAPTIC_TYPE_VISCOSE: return "Viscose";
        default: return "?";
    }
}

// audio_click_timbre_t itself now lives in audio_trigger.h (Phase 8 step 5) -- i2s_task.c
// reads it directly to pick which detent-click renderer to run, so it can no longer be a
// menu.c-private enum the way it was before.
static _Atomic audio_click_timbre_t s_ph_sound = AUDIO_TIMBRE_WOOD_TOCK;
static const char *ph_sound_name(audio_click_timbre_t s) {
    return (s == AUDIO_TIMBRE_WOOD_TOCK) ? "Wood Tock" : "Tick Thud";
}

static _Atomic float s_ph_pitch = AUDIO_CLICK_PITCH_DEFAULT;

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

// boot_usb_mode_t itself now lives in boot_mode.h (Phase 8 step 6) -- main.c reads it
// directly at startup to decide which USB personality to bring up.
static _Atomic boot_usb_mode_t s_ph_boot_mode = BOOT_USB_MODE_HID; // matches today's real default (normal boot = composite HID+CDC)
static const char *ph_boot_mode_name(boot_usb_mode_t m) {
    return (m == BOOT_USB_MODE_SERIAL) ? "Serial" : "HID";
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
    if (v < (int32_t)HAPTIC_NUM_DETENTS_MIN) v = (int32_t)HAPTIC_NUM_DETENTS_MIN;
    if (v > (int32_t)HAPTIC_NUM_DETENTS_MAX) v = (int32_t)HAPTIC_NUM_DETENTS_MAX;
    atomic_store_explicit(&s_ph_detents, v, memory_order_relaxed);
}

static void fmt_kp(char *buf, size_t n) {
    snprintf(buf, n, "%.2f", (double)atomic_load_explicit(&s_ph_kp, memory_order_relaxed));
}
static void rotate_kp(int8_t dir) {
    float v = atomic_load_explicit(&s_ph_kp, memory_order_relaxed) + dir * 0.05f;
    if (v < HAPTIC_KP_MIN) v = HAPTIC_KP_MIN;
    if (v > HAPTIC_KP_MAX) v = HAPTIC_KP_MAX;
    atomic_store_explicit(&s_ph_kp, v, memory_order_relaxed);
}

static void fmt_kd(char *buf, size_t n) {
    snprintf(buf, n, "%.3f", (double)atomic_load_explicit(&s_ph_kd, memory_order_relaxed));
}
static void rotate_kd(int8_t dir) {
    float v = atomic_load_explicit(&s_ph_kd, memory_order_relaxed) + dir * 0.005f;
    if (v < HAPTIC_KD_MIN) v = HAPTIC_KD_MIN;
    if (v > HAPTIC_KD_MAX) v = HAPTIC_KD_MAX;
    atomic_store_explicit(&s_ph_kd, v, memory_order_relaxed);
}

static void fmt_haptic_type(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_haptic_type_name(atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed)));
}
static void rotate_haptic_type(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed) + dir) % HAPTIC_TYPE_COUNT;
    if (v < 0) v += HAPTIC_TYPE_COUNT;
    atomic_store_explicit(&s_ph_haptic_type, (haptic_type_t)v, memory_order_relaxed);
}

static void fmt_sound(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_sound_name(atomic_load_explicit(&s_ph_sound, memory_order_relaxed)));
}
static void rotate_sound(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_sound, memory_order_relaxed) + dir) % AUDIO_TIMBRE_COUNT;
    if (v < 0) v += AUDIO_TIMBRE_COUNT;
    atomic_store_explicit(&s_ph_sound, (audio_click_timbre_t)v, memory_order_relaxed);
}

static void fmt_pitch(char *buf, size_t n) {
    snprintf(buf, n, "%.2fx", (double)atomic_load_explicit(&s_ph_pitch, memory_order_relaxed));
}
static void rotate_pitch(int8_t dir) {
    float v = atomic_load_explicit(&s_ph_pitch, memory_order_relaxed) + dir * 0.05f;
    if (v < AUDIO_CLICK_PITCH_MIN) v = AUDIO_CLICK_PITCH_MIN;
    if (v > AUDIO_CLICK_PITCH_MAX) v = AUDIO_CLICK_PITCH_MAX;
    atomic_store_explicit(&s_ph_pitch, v, memory_order_relaxed);
}

static void action_save_haptic(void) {
    haptic_cfg_t cfg = {
        .num_detents = (uint32_t)atomic_load_explicit(&s_ph_detents, memory_order_relaxed),
        .kp = atomic_load_explicit(&s_ph_kp, memory_order_relaxed),
        .kd = atomic_load_explicit(&s_ph_kd, memory_order_relaxed),
        .haptic_type = (int32_t)atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed),
        .sound = (int32_t)atomic_load_explicit(&s_ph_sound, memory_order_relaxed),
        .pitch = atomic_load_explicit(&s_ph_pitch, memory_order_relaxed),
    };
    config_store_save_haptic(&cfg);
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
    hid_cfg_t cfg = {
        .hid_type = (int32_t)atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed),
        .midi_channel = atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed),
    };
    config_store_save_hid(&cfg);
}

static void fmt_boot_mode(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_boot_mode_name(atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed)));
}
static void rotate_boot_mode(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed) + dir) % BOOT_USB_MODE_COUNT;
    if (v < 0) v += BOOT_USB_MODE_COUNT;
    atomic_store_explicit(&s_ph_boot_mode, (boot_usb_mode_t)v, memory_order_relaxed);
}
static void action_save_boot(void) {
    boot_cfg_t cfg = {
        .boot_mode = (int32_t)atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed),
    };
    config_store_save_boot(&cfg);
}

// --- Screens ---

static const menu_item_t s_haptic_items[] = {
    { .label = "Detents",      .kind = MENU_ITEM_VALUE,  .format_value = fmt_detents,      .on_rotate = rotate_detents },
    { .label = "Kp",           .kind = MENU_ITEM_VALUE,  .format_value = fmt_kp,           .on_rotate = rotate_kp },
    { .label = "Kd",           .kind = MENU_ITEM_VALUE,  .format_value = fmt_kd,           .on_rotate = rotate_kd },
    { .label = "Haptic Type",  .kind = MENU_ITEM_VALUE,  .format_value = fmt_haptic_type,  .on_rotate = rotate_haptic_type },
    { .label = "Haptic Sound", .kind = MENU_ITEM_VALUE,  .format_value = fmt_sound,        .on_rotate = rotate_sound },
    { .label = "Pitch",        .kind = MENU_ITEM_VALUE,  .format_value = fmt_pitch,        .on_rotate = rotate_pitch },
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

    // Phase 8 step 2: restore persisted settings over the compiled-in defaults above, if a
    // valid save exists (see config_store.c). No lock needed here -- menu_init() runs once
    // from app_main() before control_task_start()/display_task_start(), i.e. before any other
    // reader or writer of these atomics exists yet.
    haptic_cfg_t hcfg;
    if (config_store_load_haptic(&hcfg)) {
        atomic_store_explicit(&s_ph_detents, (int32_t)hcfg.num_detents, memory_order_relaxed);
        atomic_store_explicit(&s_ph_kp, hcfg.kp, memory_order_relaxed);
        atomic_store_explicit(&s_ph_kd, hcfg.kd, memory_order_relaxed);
        atomic_store_explicit(&s_ph_haptic_type, (haptic_type_t)hcfg.haptic_type, memory_order_relaxed);
        atomic_store_explicit(&s_ph_sound, (audio_click_timbre_t)hcfg.sound, memory_order_relaxed);
        atomic_store_explicit(&s_ph_pitch, hcfg.pitch, memory_order_relaxed);
    }
    hid_cfg_t icfg;
    if (config_store_load_hid(&icfg)) {
        atomic_store_explicit(&s_ph_hid_type, (ph_hid_type_t)icfg.hid_type, memory_order_relaxed);
        atomic_store_explicit(&s_ph_midi_channel, icfg.midi_channel, memory_order_relaxed);
    }
    boot_cfg_t bcfg;
    if (config_store_load_boot(&bcfg)) {
        atomic_store_explicit(&s_ph_boot_mode, (boot_usb_mode_t)bcfg.boot_mode, memory_order_relaxed);
    }
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
    void (*action_to_run)(void) = NULL;

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
                // Copy the function pointer out and run it after portEXIT_CRITICAL below, per
                // this file's own earlier warning: since Phase 8 step 2, on_action() (the
                // Save actions) does a real NVS commit, which is NOT bounded/cheap and must
                // never run under this spinlock (a portMUX critical section disables
                // interrupts on this core for its duration).
                action_to_run = it->on_action;
                break;
        }
    }
    portEXIT_CRITICAL(&s_state_mux);

    // Runs on Core 0 (control_task.c's real-time loop), outside the spinlock, but still
    // synchronously -- an NVS blob write/commit is a few ms of flash-erase/write latency, and
    // this stalls that tick of the control loop for the duration. Acceptable because it only
    // ever fires on an infrequent, user-paced button press (unlike the retired per-tick
    // string-formatting bug this file's header comment documents, which fired continuously
    // during fast rotation) -- but worth confirming on hardware there's no audible/feel
    // glitch when pressing Save.
    if (action_to_run != NULL) {
        action_to_run();
    }
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

// Phase 8 step 3 -- see menu.h's declaration comment. Called from Core 0 (control_task.c's
// real-time haptic loop) on the same atomics rotate_kp()/rotate_kd()/rotate_detents() above
// already write from Core 0's menu_input_rotate(), and format_value() already reads from
// Core 1's menu_get_render_snapshot() -- one more atomic reader needs no new synchronization,
// same lock-free convention this file already relies on for all three.
uint32_t menu_get_haptic_num_detents(void) {
    return (uint32_t)atomic_load_explicit(&s_ph_detents, memory_order_relaxed);
}

float menu_get_haptic_kp(void) {
    return atomic_load_explicit(&s_ph_kp, memory_order_relaxed);
}

float menu_get_haptic_kd(void) {
    return atomic_load_explicit(&s_ph_kd, memory_order_relaxed);
}

haptic_type_t menu_get_haptic_type(void) {
    return atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed);
}

audio_click_timbre_t menu_get_haptic_sound(void) {
    return atomic_load_explicit(&s_ph_sound, memory_order_relaxed);
}

float menu_get_haptic_pitch(void) {
    return atomic_load_explicit(&s_ph_pitch, memory_order_relaxed);
}

boot_usb_mode_t menu_get_boot_mode(void) {
    return atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed);
}
