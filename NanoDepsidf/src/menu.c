#include "menu.h"
#include "config_store.h"
#include "haptic_params.h"
#include "app_profiles/app_profiles.h"
#include "freertos/FreeRTOS.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

// --- Item/screen model ---

typedef enum {
    MENU_ITEM_SUBMENU, // enters a child screen
    MENU_ITEM_VALUE,   // enters edit mode; rendered as "label  value"
    MENU_ITEM_ACTION,  // F1 arms, a second F1 runs `action`; turning or F3 disarms
} menu_item_kind_t;
// (An older ACTION kind was for the "Save" rows; saving is F2 now -- menu_input_save().)

typedef struct menu_screen_s menu_screen_t;

typedef struct {
    const char *label;
    const char *caption;                               // small engineering name, NULL = none
    menu_item_kind_t kind;
    const menu_screen_t *submenu;                      // MENU_ITEM_SUBMENU
    void (*format_value)(char *buf, size_t buf_size);  // MENU_ITEM_VALUE
    void (*on_rotate)(int8_t direction);               // MENU_ITEM_VALUE, called while editing
    bool (*is_enabled)(void);                          // NULL = always enabled
    bool (*at_end)(int8_t direction);                  // non-wrapping value at its end that way
    void (*action)(void);                              // MENU_ITEM_ACTION, on the confirming F1
} menu_item_t;

struct menu_screen_s {
    menu_screen_id_t id;
    const char *title;
    const menu_item_t *items;
    int item_count;
    bool direct_edit;          // turn edits the focused value directly -- see menu.h
    void (*save)(void);        // F2; NULL = nothing to save on this screen
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
        case HAPTIC_TYPE_SAW: return "SAW";
        case HAPTIC_TYPE_SINE: return "SINE";
        case HAPTIC_TYPE_VISCOSE: return "VISCOSE";
        default: return "?";
    }
}

// audio_click_timbre_t itself now lives in audio_trigger.h (Phase 8 step 5) -- i2s_task.c
// reads it directly to pick which detent-click renderer to run, so it can no longer be a
// menu.c-private enum the way it was before.
static _Atomic audio_click_timbre_t s_ph_sound = AUDIO_TIMBRE_WOOD_TOCK;
static const char *ph_sound_name(audio_click_timbre_t s) {
    return (s == AUDIO_TIMBRE_WOOD_TOCK) ? "WOOD" : "THUD"; // short: shares the Main Screen status strip
}

static _Atomic float s_ph_pitch = AUDIO_CLICK_PITCH_DEFAULT;
static _Atomic int32_t s_ph_amp = AUDIO_CLICK_AMP_DEFAULT; // percent

// menu_hid_type_t lives in menu.h -- display_task.cpp reads it for the mode icon.
static _Atomic menu_hid_type_t s_ph_hid_type = MENU_HID_APP; // default until something is saved
static const char *ph_hid_type_name(menu_hid_type_t t) {
    switch (t) {
        case MENU_HID_KEYBOARD: return "KEYBOARD";
        case MENU_HID_MOUSE: return "MOUSE";
        case MENU_HID_MIDI: return "MIDI";
        case MENU_HID_APP: return "APP";
        default: return "?";
    }
}

static _Atomic int32_t s_ph_midi_channel = 1; // 1-16
static _Atomic int32_t s_ph_app_profile = 0;  // app_profiles_get() index; NVS stores the id

// boot_usb_mode_t itself now lives in boot_mode.h (Phase 8 step 6) -- main.c reads it
// directly at startup to decide which USB personality to bring up.
static _Atomic boot_usb_mode_t s_ph_boot_mode = BOOT_USB_MODE_HID; // matches today's real default (normal boot = composite HID+CDC)
// Quarter turns clockwise. Direct screen: turning rotates the screen live, F2 keeps it.
static _Atomic int32_t s_ph_rotation = 0;

static const char *ph_boot_mode_name(boot_usb_mode_t m) {
    return (m == BOOT_USB_MODE_SERIAL) ? "SERIAL" : "HID";
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
    // ".010" rather than "0.010" -- the leading zero costs a character on the Orbit ring
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%.3f", (double)atomic_load_explicit(&s_ph_kd, memory_order_relaxed));
    snprintf(buf, n, "%s", (tmp[0] == '0') ? tmp + 1 : tmp);
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

// TONE is hidden from the Haptics screen for now (AMP took its place in the ring, which holds
// six); the setting itself is still saved, restored and used by the I2S task. To bring it
// back, give it a row again.
__attribute__((unused)) static void fmt_sound(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_sound_name(atomic_load_explicit(&s_ph_sound, memory_order_relaxed)));
}
__attribute__((unused)) static void rotate_sound(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_sound, memory_order_relaxed) + dir) % AUDIO_TIMBRE_COUNT;
    if (v < 0) v += AUDIO_TIMBRE_COUNT;
    atomic_store_explicit(&s_ph_sound, (audio_click_timbre_t)v, memory_order_relaxed);
}

static void fmt_pitch(char *buf, size_t n) {
    snprintf(buf, n, "%.2fX", (double)atomic_load_explicit(&s_ph_pitch, memory_order_relaxed));
}
static void rotate_pitch(int8_t dir) {
    float v = atomic_load_explicit(&s_ph_pitch, memory_order_relaxed) + dir * 0.05f;
    if (v < AUDIO_CLICK_PITCH_MIN) v = AUDIO_CLICK_PITCH_MIN;
    if (v > AUDIO_CLICK_PITCH_MAX) v = AUDIO_CLICK_PITCH_MAX;
    atomic_store_explicit(&s_ph_pitch, v, memory_order_relaxed);
}

static void fmt_amp(char *buf, size_t n) {
    snprintf(buf, n, "%ld%%", (long)atomic_load_explicit(&s_ph_amp, memory_order_relaxed));
}
static void rotate_amp(int8_t dir) {
    int32_t v = atomic_load_explicit(&s_ph_amp, memory_order_relaxed) + dir * AUDIO_CLICK_AMP_STEP;
    if (v < AUDIO_CLICK_AMP_MIN) v = AUDIO_CLICK_AMP_MIN;
    if (v > AUDIO_CLICK_AMP_MAX) v = AUDIO_CLICK_AMP_MAX;
    atomic_store_explicit(&s_ph_amp, v, memory_order_relaxed);
}

static void action_save_haptic(void) {
    haptic_cfg_t cfg = {
        .num_detents = (uint32_t)atomic_load_explicit(&s_ph_detents, memory_order_relaxed),
        .kp = atomic_load_explicit(&s_ph_kp, memory_order_relaxed),
        .kd = atomic_load_explicit(&s_ph_kd, memory_order_relaxed),
        .haptic_type = (int32_t)atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed),
        .sound = (int32_t)atomic_load_explicit(&s_ph_sound, memory_order_relaxed),
        .pitch = atomic_load_explicit(&s_ph_pitch, memory_order_relaxed),
        .amplitude = atomic_load_explicit(&s_ph_amp, memory_order_relaxed),
    };
    config_store_save_haptic(&cfg);
}

static void fmt_hid_type(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_hid_type_name(atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed)));
}
static void rotate_hid_type(int8_t dir) {
    int pos = menu_hid_type_pos(atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed)) + dir;
    atomic_store_explicit(&s_ph_hid_type, menu_hid_type_at(pos), memory_order_relaxed);
}
static bool midi_mapping_enabled(void) {
    return atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed) == MENU_HID_MIDI;
}

static void fmt_midi_mapping(char *buf, size_t n) {
    snprintf(buf, n, "%02ld", (long)atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed));
}
static void rotate_midi_mapping(int8_t dir) {
    int32_t v = atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed) + dir;
    if (v < 1) v = 1;
    if (v > 16) v = 16;
    atomic_store_explicit(&s_ph_midi_channel, v, memory_order_relaxed);
}

static bool app_profile_enabled(void) {
    return atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed) == MENU_HID_APP;
}
static void fmt_app_profile(char *buf, size_t n) {
    snprintf(buf, n, "%s", app_profiles_get(atomic_load_explicit(&s_ph_app_profile, memory_order_relaxed))->name);
}
// The profile list stops at both ends instead of wrapping: with only a few apps, a
// wrapping carousel would show the same app on both sides.
static bool app_profile_at_end(int8_t dir) {
    int v = (int)atomic_load_explicit(&s_ph_app_profile, memory_order_relaxed);
    return dir > 0 ? v >= app_profiles_count() - 1 : v <= 0;
}
static void rotate_app_profile(int8_t dir) {
    int v = (int)atomic_load_explicit(&s_ph_app_profile, memory_order_relaxed) + dir;
    if (v < 0) v = 0;
    if (v >= app_profiles_count()) v = app_profiles_count() - 1;
    atomic_store_explicit(&s_ph_app_profile, v, memory_order_relaxed);
}

static void action_save_hid(void) {
    hid_cfg_t cfg = {
        .hid_type = (int32_t)atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed),
        .midi_channel = atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed),
    };
    config_store_save_hid(&cfg);
    config_store_save_app_profile(app_profiles_get(atomic_load_explicit(&s_ph_app_profile, memory_order_relaxed))->id);
}

static void fmt_boot_mode(char *buf, size_t n) {
    snprintf(buf, n, "%s", ph_boot_mode_name(atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed)));
}
static void rotate_boot_mode(int8_t dir) {
    int v = ((int)atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed) + dir) % BOOT_USB_MODE_COUNT;
    if (v < 0) v += BOOT_USB_MODE_COUNT;
    atomic_store_explicit(&s_ph_boot_mode, (boot_usb_mode_t)v, memory_order_relaxed);
}
static void fmt_rotation(char *buf, size_t n) {
    snprintf(buf, n, "%ld", (long)atomic_load_explicit(&s_ph_rotation, memory_order_relaxed) * 90);
}
static void rotate_rotation(int8_t dir) {
    int32_t v = (atomic_load_explicit(&s_ph_rotation, memory_order_relaxed) + dir) % MENU_DISPLAY_ROTATIONS;
    if (v < 0) v += MENU_DISPLAY_ROTATIONS;
    atomic_store_explicit(&s_ph_rotation, v, memory_order_relaxed);
}
static void action_save_display(void) {
    display_cfg_t cfg = {.rotation = atomic_load_explicit(&s_ph_rotation, memory_order_relaxed)};
    config_store_save_display(&cfg);
}

static void action_save_boot(void) {
    boot_cfg_t cfg = {
        .boot_mode = (int32_t)atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed),
    };
    config_store_save_boot(&cfg);
}

// --- Screens ---
// Labels are the Pixel UI's display names (DEVELOPMENT_PLAN.md "Pixel UI"); captions keep the
// engineering names visible in small type. Detents are "STEPS" for now -- they're due for a
// proper rename later.

static const menu_item_t s_haptic_items[MENU_HAPTIC_ROW_COUNT] = {
    [MENU_HAPTIC_ROW_STEPS] = { .label = "STEPS", .caption = "DETENTS", .kind = MENU_ITEM_VALUE, .format_value = fmt_detents,     .on_rotate = rotate_detents },
    [MENU_HAPTIC_ROW_SNAP]  = { .label = "SNAP",  .caption = "KP",      .kind = MENU_ITEM_VALUE, .format_value = fmt_kp,          .on_rotate = rotate_kp },
    [MENU_HAPTIC_ROW_DAMP]  = { .label = "DAMP",  .caption = "KD",      .kind = MENU_ITEM_VALUE, .format_value = fmt_kd,          .on_rotate = rotate_kd },
    [MENU_HAPTIC_ROW_FEEL]  = { .label = "FEEL",  .caption = "TYPE",    .kind = MENU_ITEM_VALUE, .format_value = fmt_haptic_type, .on_rotate = rotate_haptic_type },
    [MENU_HAPTIC_ROW_AMP]   = { .label = "AMP",   .caption = "AMPLITUDE", .kind = MENU_ITEM_VALUE, .format_value = fmt_amp,       .on_rotate = rotate_amp },
    [MENU_HAPTIC_ROW_PITCH] = { .label = "PITCH", .caption = "CLICK",   .kind = MENU_ITEM_VALUE, .format_value = fmt_pitch,       .on_rotate = rotate_pitch },
};
static const menu_screen_t s_haptic_screen = {
    MENU_SCREEN_HAPTIC, "Haptic Configurator", s_haptic_items, MENU_HAPTIC_ROW_COUNT, false, action_save_haptic
};

// APP -> F1 opens a screen of its own for choosing the app profile (a carousel of app
// icons), rather than an inline row -- it scales past a handful of apps.
static const menu_item_t s_app_profile_items[] = {
    { .label = "PROFILE", .kind = MENU_ITEM_VALUE, .format_value = fmt_app_profile, .on_rotate = rotate_app_profile, .at_end = app_profile_at_end },
};
static const menu_screen_t s_app_profile_screen = {
    MENU_SCREEN_APP_PROFILE, "App Profile", s_app_profile_items, 1, true, action_save_hid
};

static const menu_item_t s_hid_items[] = {
    { .label = "PROFILES", .kind = MENU_ITEM_VALUE,   .format_value = fmt_hid_type,     .on_rotate = rotate_hid_type },
    { .label = "CHANNEL",  .kind = MENU_ITEM_VALUE,   .format_value = fmt_midi_mapping, .on_rotate = rotate_midi_mapping, .is_enabled = midi_mapping_enabled },
    { .label = "PROFILE",  .kind = MENU_ITEM_SUBMENU, .submenu = &s_app_profile_screen, .format_value = fmt_app_profile, .is_enabled = app_profile_enabled },
};
static const menu_screen_t s_hid_screen = {
    MENU_SCREEN_HID, "Profiles", s_hid_items, sizeof(s_hid_items) / sizeof(s_hid_items[0]), true, action_save_hid
};

static const menu_item_t s_boot_items[] = {
    { .label = "USB MODE", .kind = MENU_ITEM_VALUE, .format_value = fmt_boot_mode, .on_rotate = rotate_boot_mode },
};
static const menu_screen_t s_boot_screen = {
    MENU_SCREEN_BOOT, "Boot USB Mode", s_boot_items, sizeof(s_boot_items) / sizeof(s_boot_items[0]), true, action_save_boot
};

static const menu_item_t s_display_items[] = {
    { .label = "ROTATION", .kind = MENU_ITEM_VALUE, .format_value = fmt_rotation, .on_rotate = rotate_rotation },
};
static const menu_screen_t s_display_screen = {
    MENU_SCREEN_DISPLAY, "Display", s_display_items, 1, true, action_save_display
};

// DEVICE: USB power is drawn from pd_status.h directly (nothing to choose); the one item is
// RECALIBRATE. Its action only raises a flag -- control_task.c owns the motor, so it does the
// work there (see menu_take_recalibrate_request()).
static _Atomic bool s_recal_request = false;
static void action_recalibrate(void) {
    atomic_store_explicit(&s_recal_request, true, memory_order_relaxed);
}
static const menu_item_t s_device_items[] = {
    { .label = "RECALIBRATE", .kind = MENU_ITEM_ACTION, .action = action_recalibrate },
};
static const menu_screen_t s_device_screen = {
    MENU_SCREEN_DEVICE, "Device", s_device_items, 1, false, NULL
};

static const menu_item_t s_root_items[] = {
    { .label = "PROFILES",  .kind = MENU_ITEM_SUBMENU, .submenu = &s_hid_screen },
    { .label = "HAPTICS",   .kind = MENU_ITEM_SUBMENU, .submenu = &s_haptic_screen },
    { .label = "DISPLAY",   .kind = MENU_ITEM_SUBMENU, .submenu = &s_display_screen },
    { .label = "BOOT MODE", .kind = MENU_ITEM_SUBMENU, .submenu = &s_boot_screen },
    { .label = "DEVICE",    .kind = MENU_ITEM_SUBMENU, .submenu = &s_device_screen },
};
static const menu_screen_t s_root_screen = {
    MENU_SCREEN_ROOT, "", s_root_items, sizeof(s_root_items) / sizeof(s_root_items[0]), false, NULL
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
static bool s_armed = false; // a MENU_ITEM_ACTION waiting for its confirming F1
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

// --- Settings snapshots (Pixel UI) ---
// One plain copy of every setting atomic. Used three ways:
//   - s_saved: what NVS holds -- compared against the live atomics for the "unsaved" (dirty)
//     cue, and restored from when a direct screen (HID / Boot mode) is left without saving.
//   - s_undo: taken when a Haptic edit starts, restored by F3 (cancel).
// All capture/restore work is a handful of atomic loads/stores -- bounded and cheap, safe on
// Core 0's real-time loop. s_saved is read from Core 1 (snapshot) and written from Core 0
// (save), so it's only ever copied under s_state_mux.

typedef struct {
    int32_t detents;
    float kp;
    float kd;
    haptic_type_t haptic_type;
    audio_click_timbre_t sound;
    float pitch;
    int32_t amp;
    menu_hid_type_t hid_type;
    int32_t midi_channel;
    int32_t app_profile;
    boot_usb_mode_t boot_mode;
    int32_t rotation;
} settings_t;

static settings_t s_saved;
static settings_t s_undo;
static _Atomic uint32_t s_save_count = 0;

static void settings_capture(settings_t *s) {
    s->detents = atomic_load_explicit(&s_ph_detents, memory_order_relaxed);
    s->kp = atomic_load_explicit(&s_ph_kp, memory_order_relaxed);
    s->kd = atomic_load_explicit(&s_ph_kd, memory_order_relaxed);
    s->haptic_type = atomic_load_explicit(&s_ph_haptic_type, memory_order_relaxed);
    s->sound = atomic_load_explicit(&s_ph_sound, memory_order_relaxed);
    s->pitch = atomic_load_explicit(&s_ph_pitch, memory_order_relaxed);
    s->amp = atomic_load_explicit(&s_ph_amp, memory_order_relaxed);
    s->hid_type = atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed);
    s->midi_channel = atomic_load_explicit(&s_ph_midi_channel, memory_order_relaxed);
    s->app_profile = atomic_load_explicit(&s_ph_app_profile, memory_order_relaxed);
    s->boot_mode = atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed);
    s->rotation = atomic_load_explicit(&s_ph_rotation, memory_order_relaxed);
}

static void settings_restore(const settings_t *s) {
    atomic_store_explicit(&s_ph_detents, s->detents, memory_order_relaxed);
    atomic_store_explicit(&s_ph_kp, s->kp, memory_order_relaxed);
    atomic_store_explicit(&s_ph_kd, s->kd, memory_order_relaxed);
    atomic_store_explicit(&s_ph_haptic_type, s->haptic_type, memory_order_relaxed);
    atomic_store_explicit(&s_ph_sound, s->sound, memory_order_relaxed);
    atomic_store_explicit(&s_ph_pitch, s->pitch, memory_order_relaxed);
    atomic_store_explicit(&s_ph_amp, s->amp, memory_order_relaxed);
    atomic_store_explicit(&s_ph_hid_type, s->hid_type, memory_order_relaxed);
    atomic_store_explicit(&s_ph_midi_channel, s->midi_channel, memory_order_relaxed);
    atomic_store_explicit(&s_ph_app_profile, s->app_profile, memory_order_relaxed);
    atomic_store_explicit(&s_ph_boot_mode, s->boot_mode, memory_order_relaxed);
    atomic_store_explicit(&s_ph_rotation, s->rotation, memory_order_relaxed);
}

// Copies only the fields a screen owns (what that screen's save writes to NVS).
static void settings_copy_group(settings_t *dst, const settings_t *src, menu_screen_id_t group) {
    switch (group) {
        case MENU_SCREEN_HAPTIC:
            dst->detents = src->detents;
            dst->kp = src->kp;
            dst->kd = src->kd;
            dst->haptic_type = src->haptic_type;
            dst->sound = src->sound;
            dst->pitch = src->pitch;
            dst->amp = src->amp;
            break;
        case MENU_SCREEN_HID:
            dst->hid_type = src->hid_type;
            dst->midi_channel = src->midi_channel;
            dst->app_profile = src->app_profile;
            break;
        case MENU_SCREEN_APP_PROFILE:
            dst->app_profile = src->app_profile;
            break;
        case MENU_SCREEN_BOOT:
            dst->boot_mode = src->boot_mode;
            break;
        case MENU_SCREEN_DISPLAY:
            dst->rotation = src->rotation;
            break;
        default:
            break;
    }
}

static bool settings_group_differs(const settings_t *a, const settings_t *b, menu_screen_id_t group) {
    switch (group) {
        case MENU_SCREEN_HAPTIC:
            return a->detents != b->detents || a->kp != b->kp || a->kd != b->kd
                || a->haptic_type != b->haptic_type || a->sound != b->sound || a->pitch != b->pitch || a->amp != b->amp;
        case MENU_SCREEN_HID:
            return a->hid_type != b->hid_type || a->midi_channel != b->midi_channel
                || a->app_profile != b->app_profile;
        case MENU_SCREEN_APP_PROFILE:
            return a->app_profile != b->app_profile;
        case MENU_SCREEN_BOOT:
            return a->boot_mode != b->boot_mode;
        case MENU_SCREEN_DISPLAY:
            return a->rotation != b->rotation;
        default:
            return false;
    }
}

// What F2 on a screen saves (and what its "F2 SAVE" hint tracks). The profile screen saves
// the whole HID group -- choosing a profile there commits the APP choice with it -- while its
// F3 only reverts the profile itself (settings_copy_group by the screen's own id).
static menu_screen_id_t save_group(menu_screen_id_t id) {
    return id == MENU_SCREEN_APP_PROFILE ? MENU_SCREEN_HID : id;
}

// Puts a direct screen's fields back to their saved values (leaving without F2). Core 0,
// caller holds s_state_mux (s_saved is read here).
static void revert_group_locked(menu_screen_id_t group) {
    settings_t cur;
    settings_capture(&cur);
    settings_copy_group(&cur, &s_saved, group);
    settings_restore(&cur);
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
        atomic_store_explicit(&s_ph_amp, hcfg.amplitude, memory_order_relaxed);
    }
    hid_cfg_t icfg;
    if (config_store_load_hid(&icfg)) {
        atomic_store_explicit(&s_ph_hid_type, (menu_hid_type_t)icfg.hid_type, memory_order_relaxed);
        atomic_store_explicit(&s_ph_midi_channel, icfg.midi_channel, memory_order_relaxed);
    }
    char app_id[16];
    if (config_store_load_app_profile(app_id, sizeof(app_id))) {
        int idx = app_profiles_find(app_id);
        if (idx >= 0) atomic_store_explicit(&s_ph_app_profile, idx, memory_order_relaxed);
    }
    boot_cfg_t bcfg;
    if (config_store_load_boot(&bcfg)) {
        atomic_store_explicit(&s_ph_boot_mode, (boot_usb_mode_t)bcfg.boot_mode, memory_order_relaxed);
    }
    display_cfg_t dcfg;
    if (config_store_load_display(&dcfg)) {
        atomic_store_explicit(&s_ph_rotation, dcfg.rotation, memory_order_relaxed);
    }

    // Whatever is live now is, by definition, what's saved (or the defaults, if nothing was)
    // -- the baseline for the dirty cue.
    settings_capture(&s_saved);
}


void menu_input_toggle_open(void) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0) {
        int idx = first_enabled_index(&s_root_screen);
        s_stack[0].screen = &s_root_screen;
        s_stack[0].selected_index = (idx < 0) ? 0 : idx;
        s_stack_depth = 1;
    } else {
        // Closing from a direct screen discards its unsaved choice; a Haptic edit in
        // progress is kept as-is (live, unsaved), same as confirming it.
        for (int i = s_stack_depth - 1; i >= 0; i--) {
            if (s_stack[i].screen->direct_edit) {
                revert_group_locked(s_stack[i].screen->id);
            }
        }
        s_stack_depth = 0;
        s_editing = false;
    }
    s_armed = false;
    portEXIT_CRITICAL(&s_state_mux);
}

void menu_input_back(void) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0) {
        // no-op
    } else if (s_armed) {
        s_armed = false; // cancel the action, stay on the screen
    } else if (s_editing) {
        // Cancel: put back what the value was when the edit started.
        settings_restore(&s_undo);
        s_editing = false;
    } else {
        const menu_screen_t *top = s_stack[s_stack_depth - 1].screen;
        if (top->direct_edit) {
            revert_group_locked(top->id);
        }
        s_stack_depth = (s_stack_depth > 1) ? s_stack_depth - 1 : 0; // at the top level, back = close
    }
    portEXIT_CRITICAL(&s_state_mux);
}

void menu_input_select(void) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0) {
        portEXIT_CRITICAL(&s_state_mux);
        return;
    }
    menu_stack_frame_t *top = &s_stack[s_stack_depth - 1];
    const menu_item_t *it = &top->screen->items[top->selected_index];
    void (*action)(void) = NULL;

    if (it->kind == MENU_ITEM_ACTION) {
        if (s_armed) action = it->action; // the confirming press
        s_armed = !s_armed;
    } else if (top->screen->direct_edit) {
        // Direct screens: F1 moves to the next field (HID type <-> MIDI channel) -- or, when
        // that field is a submenu (APP -> PROFILE), opens it; focus stays on this screen's
        // own field.
        int next = step_index(top->screen, top->selected_index, 1);
        const menu_item_t *nit = &top->screen->items[next];
        if (nit->kind == MENU_ITEM_SUBMENU) {
            if (s_stack_depth < MENU_MAX_DEPTH && nit->submenu != NULL) {
                int idx = first_enabled_index(nit->submenu);
                s_stack[s_stack_depth].screen = nit->submenu;
                s_stack[s_stack_depth].selected_index = (idx < 0) ? 0 : idx;
                s_stack_depth++;
            }
        } else {
            top->selected_index = next;
        }
    } else if (s_editing) {
        s_editing = false; // confirm -- the value is already live
    } else if (it->kind == MENU_ITEM_SUBMENU) {
        if (s_stack_depth < MENU_MAX_DEPTH && it->submenu != NULL) {
            int idx = first_enabled_index(it->submenu);
            s_stack[s_stack_depth].screen = it->submenu;
            s_stack[s_stack_depth].selected_index = (idx < 0) ? 0 : idx;
            s_stack_depth++;
        }
    } else if (it->kind == MENU_ITEM_VALUE) {
        settings_capture(&s_undo);
        s_editing = true;
    }
    portEXIT_CRITICAL(&s_state_mux);
    if (action != NULL) action(); // outside the spinlock, like F2's save
}

void menu_input_save(void) {
    void (*save)(void) = NULL;
    menu_screen_id_t group = MENU_SCREEN_NONE;

    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth > 0) {
        const menu_screen_t *screen = s_stack[s_stack_depth - 1].screen;
        settings_t cur;
        settings_capture(&cur);
        if (screen->save != NULL && settings_group_differs(&cur, &s_saved, save_group(screen->id))) {
            save = screen->save;
            group = save_group(screen->id);
        }
        s_editing = false; // saving also confirms an edit in progress
    }
    portEXIT_CRITICAL(&s_state_mux);

    // The NVS commit (a few ms of flash erase/write) runs outside the spinlock -- a portMUX
    // critical section disables interrupts on this core for its duration. This still stalls
    // one tick of control_task.c's loop, as the old "Save" row did; acceptable for an
    // infrequent, user-paced press. Skipped entirely when nothing changed, so an idle F2
    // costs no flash wear.
    if (save != NULL) {
        settings_t cur;
        settings_capture(&cur);
        save();
        portENTER_CRITICAL(&s_state_mux);
        settings_copy_group(&s_saved, &cur, group);
        portEXIT_CRITICAL(&s_state_mux);
        atomic_fetch_add_explicit(&s_save_count, 1, memory_order_relaxed);
    }
}

void menu_input_rotate(int8_t direction) {
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth == 0 || direction == 0) {
        portEXIT_CRITICAL(&s_state_mux);
        return;
    }
    menu_stack_frame_t *top = &s_stack[s_stack_depth - 1];
    s_armed = false; // turning away cancels a pending action
    if (s_editing || top->screen->direct_edit) {
        const menu_item_t *it = &top->screen->items[top->selected_index];
        if (it->on_rotate != NULL) {
            it->on_rotate(direction); // atomic store(s) only -- see field callbacks above
        }
    } else {
        top->selected_index = step_index(top->screen, top->selected_index, direction);
    }
    portEXIT_CRITICAL(&s_state_mux);
}

bool menu_at_end(int8_t direction) {
    bool end = false;
    portENTER_CRITICAL(&s_state_mux);
    if (s_stack_depth > 0) {
        const menu_stack_frame_t *top = &s_stack[s_stack_depth - 1];
        const menu_item_t *it = &top->screen->items[top->selected_index];
        if ((s_editing || top->screen->direct_edit) && it->at_end != NULL) end = it->at_end(direction);
    }
    portEXIT_CRITICAL(&s_state_mux);
    return end;
}

bool menu_take_recalibrate_request(void) {
    // Cheap load first: this runs every tick of control_task.c's loop.
    if (!atomic_load_explicit(&s_recal_request, memory_order_relaxed)) return false;
    return atomic_exchange_explicit(&s_recal_request, false, memory_order_relaxed);
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
    menu_stack_frame_t top_copy = { 0 };
    int depth_copy;
    bool editing_copy, armed_copy;
    settings_t saved_copy;

    portENTER_CRITICAL(&s_state_mux);
    depth_copy = s_stack_depth;
    editing_copy = s_editing;
    armed_copy = s_armed;
    if (depth_copy > 0) {
        top_copy = s_stack[depth_copy - 1];
    }
    saved_copy = s_saved;
    portEXIT_CRITICAL(&s_state_mux);

    menu_render_snapshot_t snap = { 0 };
    snap.open = (depth_copy > 0);
    snap.editing = editing_copy;
    snap.selected = -1;
    snap.screen = MENU_SCREEN_NONE;
    snap.save_count = atomic_load_explicit(&s_save_count, memory_order_relaxed);

    if (snap.open) {
        const menu_screen_t *screen = top_copy.screen;
        snap.screen = screen->id;
        snprintf(snap.title, sizeof(snap.title), "%s", screen->title);

        settings_t cur;
        settings_capture(&cur);
        snap.dirty = settings_group_differs(&cur, &saved_copy, save_group(screen->id));

        int row = 0;
        for (int i = 0; i < screen->item_count && row < MENU_MAX_VISIBLE_ITEMS; i++) {
            if (!item_enabled(screen, i)) {
                continue; // disabled items are skipped entirely, not shown greyed-out
            }
            const menu_item_t *it = &screen->items[i];
            menu_render_row_t *r = &snap.rows[row];
            snprintf(r->label, sizeof(r->label), "%s", it->label);
            snprintf(r->caption, sizeof(r->caption), "%s", it->caption ? it->caption : "");
            if (it->kind == MENU_ITEM_VALUE && it->format_value != NULL) {
                it->format_value(r->value, sizeof(r->value));
            } else if (it->kind == MENU_ITEM_ACTION) {
                bool armed = armed_copy && i == top_copy.selected_index;
                snprintf(r->value, sizeof(r->value), "%s", armed ? MENU_RECAL_ARMED : "");
            } else {
                r->value[0] = '\0'; // submenu rows have nothing to show on the right
            }
            r->selected = (i == top_copy.selected_index);
            if (r->selected) {
                snap.selected = row;
            }
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

float menu_get_click_amplitude(void) {
    return atomic_load_explicit(&s_ph_amp, memory_order_relaxed) / 100.0f;
}

boot_usb_mode_t menu_get_boot_mode(void) {
    return atomic_load_explicit(&s_ph_boot_mode, memory_order_relaxed);
}

int32_t menu_get_app_profile(void) {
    return atomic_load_explicit(&s_ph_app_profile, memory_order_relaxed);
}

int32_t menu_get_display_rotation(void) {
    return atomic_load_explicit(&s_ph_rotation, memory_order_relaxed);
}

menu_hid_type_t menu_get_hid_type(void) {
    return atomic_load_explicit(&s_ph_hid_type, memory_order_relaxed);
}
