#pragma once
// App profile: everything APP mode needs to drive one application -- what the knob and
// F1-F4 send, how the knob feels while doing it, and what the screen shows. Pure data: the
// engine (app_mode.c), the menu (PROFILE row) and the screens read it; a profile never
// contains code. Adding an app = one new file in this folder + one line in app_profiles.c.
//
// The same layout is meant to become the uploaded-profile format later (DEVELOPMENT_PLAN.md
// "App profiles"), so it carries a version from the start.
//
// Key codes / modifiers / mouse buttons are TinyUSB's HID constants (class/hid/hid.h:
// HID_KEY_*, KEYBOARD_MODIFIER_*, MOUSE_BUTTON_*). Keys are USB key *positions*, so
// shortcuts assume a US keyboard layout (on German QWERTZ, Z and Y swap).

#include <stdbool.h>
#include <stdint.h>
#include "../haptic_params.h" // src/ has no include dir of its own; paths are relative

#define APP_PROFILE_VERSION 1

typedef enum {
    APP_ACT_NONE = 0,
    // Knob turn -> pointer drag: `buttons` (+ `modifier`) held while the knob moves the
    // pointer on one axis. Starts once the knob has moved a few px, so a held key that never
    // turns sends nothing. On the knob-alone slot it lets go once the knob rests.
    APP_ACT_DRAG,
    // Knob turn -> mouse wheel, one step per detent, with `modifier` held (e.g. Cmd = zoom).
    APP_ACT_WHEEL,
    // Knob turn -> one key tap per detent: `cw` one way, `ccw` the other.
    APP_ACT_KEYS,
    // Key press -> one key tap (`cw`). F1-F3 only: F4 is also the menu key, so it has to
    // be a turn action (or NONE).
    APP_ACT_TAP,
} app_action_kind_t;

// What the Main Screen's middle shows in APP mode: the live action as large text, or a 3D
// shape that follows the knob (micro-interactions, DEVELOPMENT_PLAN.md "Plasticity").
typedef enum { APP_VISUAL_LABEL = 0, APP_VISUAL_SHAPE } app_visual_t;
typedef enum { APP_SHAPE_CUBE = 0, APP_SHAPE_PYRAMID, APP_SHAPE_OCTA } app_shape_t;
// How the shape is drawn (preview styles R3C / R2C / R4A).
typedef enum { APP_SHAPE_STYLE_SELECTED_FACE = 0, APP_SHAPE_STYLE_CAD_GRIPS, APP_SHAPE_STYLE_THICK } app_shape_style_t;
// What an action does to the shape: zoom through nested copies, turn it, slide it, or flash.
typedef enum { APP_FX_NONE = 0, APP_FX_ZOOM, APP_FX_ORBIT, APP_FX_PAN, APP_FX_FLASH } app_fx_t;

typedef struct {
    uint8_t modifier; // KEYBOARD_MODIFIER_* bits
    uint8_t keycode;  // HID_KEY_*
} app_key_t;

typedef struct {
    app_action_kind_t kind;
    const char *label;   // shown large mid-screen while this action is live ("ZOOM")
    // DRAG
    uint8_t buttons;     // MOUSE_BUTTON_* bits
    uint8_t modifier;    // KEYBOARD_MODIFIER_* held during the drag / wheel
    bool axis_y;         // pointer travel on y instead of x
    float px_per_rad;    // pointer px per radian of knob travel
    // DRAG + WHEEL: +1 / -1, flips which way the knob drives the app
    int8_t sign;
    // KEYS (cw/ccw) and TAP (cw)
    app_key_t cw, ccw;
    // Feel while this action is live. detents 0 = the Haptics menu's STEPS value.
    haptic_type_t feel;
    uint16_t detents;
    app_fx_t fx;         // APP_VISUAL_SHAPE: what this action does to the shape
} app_action_t;

// Slots: what each input does. KNOB = turning with no key held; F1/F2/F4 = turning while
// that key is held (or a TAP on press); F3 = usually a TAP. Holding F4 ~0.7s without
// turning always opens the menu, in every profile.
typedef enum {
    APP_SLOT_KNOB = 0,
    APP_SLOT_F1,
    APP_SLOT_F2,
    APP_SLOT_F3,
    APP_SLOT_F4,
    APP_SLOT_COUNT,
} app_slot_t;

typedef struct {
    uint16_t version;          // APP_PROFILE_VERSION
    const char *id;            // stable id, stored in NVS ("figma")
    const char *name;          // shown in the status bar and the PROFILE row ("FIGMA")
    const uint8_t *icon24;     // 24x24 RGB565 BE status-bar icon (app_icons.h), or NULL
    const uint8_t *icon48;     // 48x48 RGB565 BE icon for the PROFILE screen, or NULL
    const char *legend[4];     // under the F1-F4 keycaps, <= 5 chars
    app_visual_t visual;       // Main Screen middle (LABEL unless set)
    app_shape_t shape;         // APP_VISUAL_SHAPE: which shape
    app_shape_style_t shape_style; // ...and how it's drawn
    bool shape_stepped;        // show only clean poses: 32 per turn, zoom in 1/8 doublings, pan in 2px
    app_action_t slot[APP_SLOT_COUNT];
} app_profile_t;
