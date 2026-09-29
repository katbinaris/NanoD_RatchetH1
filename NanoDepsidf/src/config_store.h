#pragma once
#include <stdint.h>
#include <stdbool.h>

// Phase 8 step 2: NVS-backed persistence for the three settings groups exposed by menu.c,
// following foc_calibration.c's existing load/save pattern (one blob per namespace, sanity-
// checked on load so corrupted/partial NVS data is never trusted over a safe default).
// Scaffolding only -- menu.c's placeholder atomics are the only thing wired to this so far
// (loaded once at boot in menu_init(), saved on each screen's "Save" action). Nothing in
// control_task.c/usb_task.c/main.c reads any of this yet -- that real wiring is Phase 8
// steps 3/6/7, tracked separately in DEVELOPMENT_PLAN.md.

typedef struct {
    uint32_t num_detents;
    float kp;
    float kd;
    int32_t haptic_type; // haptic_params.h's haptic_type_t, stored as a plain int
    int32_t sound;       // audio_trigger.h's audio_click_timbre_t, stored as a plain int
    float pitch;         // audio_trigger.h's AUDIO_CLICK_PITCH_* multiplier -- added after
                          // haptic_type/sound existed; a blob saved before this field existed
                          // is a different `sizeof`, so config_store_load_haptic() naturally
                          // rejects it as wrong-size and the caller's defaults apply instead
                          // of a silent garbage read -- no explicit versioning needed, this
                          // is exactly the failure mode the exact-size check already handles.
} haptic_cfg_t;

typedef struct {
    int32_t hid_type;     // menu.h's menu_hid_type_t
    int32_t midi_channel; // 1-16
} hid_cfg_t;

typedef struct {
    int32_t boot_mode; // boot_mode.h's boot_usb_mode_t, stored as a plain int
} boot_cfg_t;

typedef struct {
    int32_t rotation; // quarter turns clockwise, 0-3
} display_cfg_t;

// Each returns false (leaving *out untouched) if the namespace doesn't exist yet (normal on
// first boot) or the stored blob fails a basic sanity check (wrong size, non-finite float,
// enum field out of range) -- callers should keep their own compiled-in default in that case,
// same as foc_calibration_load()'s caller does. Assumes nvs_flash_init() has already been
// called (done once in app_main(), before menu_init()).
bool config_store_load_haptic(haptic_cfg_t *out);
bool config_store_load_hid(hid_cfg_t *out);
bool config_store_load_boot(boot_cfg_t *out);
bool config_store_load_display(display_cfg_t *out);

void config_store_save_haptic(const haptic_cfg_t *cfg);
void config_store_save_hid(const hid_cfg_t *cfg);
void config_store_save_boot(const boot_cfg_t *cfg);
void config_store_save_display(const display_cfg_t *cfg);

// APP mode's profile, stored by its id string ("figma") next to hid_cfg -- a separate key
// rather than a new hid_cfg field, so blobs saved before profiles existed still load. The
// id (not an index) keeps a saved choice valid when profiles are added or reordered.
bool config_store_load_app_profile(char *id, size_t len);
void config_store_save_app_profile(const char *id);
