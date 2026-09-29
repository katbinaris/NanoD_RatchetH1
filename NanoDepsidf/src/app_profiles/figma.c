// Figma (design). Standard profile: keyboard shortcuts and the wheel only -- no plugin (a
// Figma plugin talking to the knob is later scope, DEVELOPMENT_PLAN.md). Shortcuts per
// Figma's docs: Cmd + wheel zooms, Cmd+Z / Shift+Cmd+Z undo/redo, Tab / Shift+Tab step
// through layers, Shift+2 zooms to the selection, arrows nudge 1px. macOS modifiers.
//
// Everything here is a step, so it runs with detents (unlike Plasticity's smooth drags):
// one detent = one zoom step, one history step, one layer, one pixel. Prior art: Work
// Louder's Figma Creator Micro puts undo/redo on a dial the same way.
#include "app_profile.h"
#include "icons/app_icons.h"
#include "class/hid/hid.h"

#define CMD KEYBOARD_MODIFIER_LEFTGUI
#define SHIFT KEYBOARD_MODIFIER_LEFTSHIFT

const app_profile_t app_profile_figma = {
    .version = APP_PROFILE_VERSION,
    .id = "figma",
    .name = "FIGMA",
    .icon24 = app_icon_figma_24,
    .icon48 = app_icon_figma_48,
    .legend = {"UNDO", "LAYER", "FOCUS", "NUDGE"},
    .slot = {
        // Wheel up with Cmd = zoom in. Flip `sign` if the knob zooms the wrong way.
        [APP_SLOT_KNOB] = {
            .kind = APP_ACT_WHEEL, .label = "ZOOM", .modifier = CMD, .sign = 1,
            .feel = HAPTIC_TYPE_SAW, .detents = 16,
        },
        [APP_SLOT_F1] = {
            .kind = APP_ACT_KEYS, .label = "UNDO/REDO",
            .cw = {CMD | SHIFT, HID_KEY_Z}, .ccw = {CMD, HID_KEY_Z},
            .feel = HAPTIC_TYPE_SAW, .detents = 12,
        },
        [APP_SLOT_F2] = {
            .kind = APP_ACT_KEYS, .label = "LAYERS",
            .cw = {0, HID_KEY_TAB}, .ccw = {SHIFT, HID_KEY_TAB},
            .feel = HAPTIC_TYPE_SAW, .detents = 12,
        },
        [APP_SLOT_F3] = {
            .kind = APP_ACT_TAP, .label = "FOCUS",
            .cw = {SHIFT, HID_KEY_2},
        },
        // Fine detents: one per pixel.
        [APP_SLOT_F4] = {
            .kind = APP_ACT_KEYS, .label = "NUDGE",
            .cw = {0, HID_KEY_ARROW_RIGHT}, .ccw = {0, HID_KEY_ARROW_LEFT},
            .feel = HAPTIC_TYPE_SAW, .detents = 36,
        },
    },
};
