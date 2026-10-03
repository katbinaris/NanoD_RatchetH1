// MUSIC: the knob is a volume dial, the keys drive the player. Pure HID Consumer-page usages
// (APP_ACT_MEDIA), handled by the OS itself -- no app focus, no host software, and the macOS
// volume OSD follows the knob.
#include "app_profile.h"
#include "icons/app_icons.h"
#include "class/hid/hid.h"

// HID Consumer page usages (all <= 0xFF, see app_profile.h APP_ACT_MEDIA).
#define MEDIA_PLAY_PAUSE 0xCD
#define MEDIA_NEXT 0xB5
#define MEDIA_PREV 0xB6
#define MEDIA_VOL_UP 0xE9
#define MEDIA_VOL_DOWN 0xEA

// macOS: Shift+Option + a volume key moves a quarter step -- 64 steps across the range
// instead of 16, so one click of the knob is ~1.6%. Dropped when BINDINGS = PC.
#define FINE (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_LEFTALT)

const app_profile_t app_profile_music = {
    .version = APP_PROFILE_VERSION,
    .id = "music",
    .name = "MUSIC",
    .icon24 = app_icon_music_24,
    .icon48 = app_icon_music_48,
    .legend = {"PLAY", "PREV", "NEXT", "MENU"},
    .slot = {
        [APP_SLOT_KNOB] = {
            .kind = APP_ACT_MEDIA, .label = "VOLUME",
            .cw = {FINE, MEDIA_VOL_UP}, .ccw = {FINE, MEDIA_VOL_DOWN},
            .feel = HAPTIC_TYPE_SAW, .detents = 36,
        },
        [APP_SLOT_F1] = {.kind = APP_ACT_MEDIA, .label = "PLAY", .cw = {0, MEDIA_PLAY_PAUSE}},
        [APP_SLOT_F2] = {.kind = APP_ACT_MEDIA, .label = "PREV", .cw = {0, MEDIA_PREV}},
        [APP_SLOT_F3] = {.kind = APP_ACT_MEDIA, .label = "NEXT", .cw = {0, MEDIA_NEXT}},
        // F4: long press = menu, as in every profile.
    },
};
