// Plasticity (3D CAD). Its default viewport navigation (doc.plasticity.xyz, "Operating the
// 3D viewport"): middle-drag orbits, right-drag pans, Ctrl + middle-drag zooms continuously.
// Tested on hardware against a browser stand-in viewport (the license had expired).
//
// History: zoom started as the wheel (a visible jump per step; macOS ignores HID
// high-resolution wheel reports) -- the continuous Ctrl + middle-drag replaced it. F3 was
// Alt + middle-click ("center the view on the cursor"), which re-centers rather than setting
// an orbit pivot in place -- it's Undo now. Drags run the smooth VISCOSE feel throughout.
#include "app_profile.h"
#include "icons/app_icons.h"
#include "class/hid/hid.h"

#define PX_PER_RAD 120.0f // ~750px of pointer travel per knob turn

#define ZOOM {                                                                 \
    .kind = APP_ACT_DRAG, .label = "ZOOM",                                     \
    .buttons = MOUSE_BUTTON_MIDDLE, .modifier = KEYBOARD_MODIFIER_LEFTCTRL,    \
    .axis_y = true, .px_per_rad = PX_PER_RAD, .sign = -1,                      \
    .feel = HAPTIC_TYPE_VISCOSE, .fx = APP_FX_ZOOM,                            \
}

const app_profile_t app_profile_plasticity = {
    .version = APP_PROFILE_VERSION,
    .id = "plasticity",
    .name = "PLASTICITY",
    .icon24 = app_icon_plasticity_24,
    .icon48 = app_icon_plasticity_48,
    .legend = {"ZOOM", "ORBIT", "UNDO", "PAN"},
    // The demo visual: an isometric pyramid chained to the knob (preview rounds 1-4,
    // https://claude.ai/artifact/9x5URCUiMgJPqHDDtFeZWj). Style: R4A "Thick" -- chosen after
    // 1px CAD grips (R2C) and "Selected face" (R3C) read too thin and shimmered on hardware.
    // Stepped motion keeps every frame a clean pose.
    .visual = APP_VISUAL_SHAPE,
    .shape = APP_SHAPE_PYRAMID,
    .shape_style = APP_SHAPE_STYLE_THICK,
    .shape_stepped = true,
    .slot = {
        [APP_SLOT_KNOB] = ZOOM,
        [APP_SLOT_F1] = ZOOM,
        [APP_SLOT_F2] = {
            .kind = APP_ACT_DRAG, .label = "ORBIT",
            .buttons = MOUSE_BUTTON_MIDDLE, .px_per_rad = PX_PER_RAD, .sign = 1,
            .feel = HAPTIC_TYPE_VISCOSE, .fx = APP_FX_ORBIT,
        },
        // Cmd+Z for macOS (HID Left GUI = Cmd; KEYBOARD_MODIFIER_LEFTCTRL on Windows).
        [APP_SLOT_F3] = {
            .kind = APP_ACT_TAP, .label = "UNDO",
            .cw = {KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_Z}, .fx = APP_FX_FLASH,
        },
        [APP_SLOT_F4] = {
            .kind = APP_ACT_DRAG, .label = "PAN",
            .buttons = MOUSE_BUTTON_RIGHT, .px_per_rad = PX_PER_RAD, .sign = 1,
            .feel = HAPTIC_TYPE_VISCOSE, .fx = APP_FX_PAN,
        },
    },
};
