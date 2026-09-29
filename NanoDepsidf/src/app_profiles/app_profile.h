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
#include <stddef.h>
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
    // Hold the key -> the command wheel (the profile's `rings`): turning picks a command, one
    // detent each, and letting go runs it. The first entry of every ring is "cancel". Other
    // F keys tapped while it's held jump to the ring bound to them. F1-F3 only.
    APP_ACT_COMMANDS,
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
    // KEYS / WHEEL / DRAG: sent on release when the key was tapped (let go quickly without
    // turning) -- one key, two jobs. keycode 0 = none.
    app_key_t tap;
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

// --- Command wheel ---
// Each command has a small illustration: a 120x64 card with a mini canvas (left, x 4..60)
// and a mini layers panel (right), animated through keyframes. It is *described*, not stored
// as pixels: a few shapes per keyframe, drawn crisp at runtime (ui_cards.cpp), so a command
// costs a few hundred bytes instead of ~15KB of frames. Canvas elements are clipped to the
// canvas pane, so moving them past its edge (a pan) is fine.
typedef enum {
    APP_EL_BOX = 0,  // filled rect, corners cut (x, y, w, h)
    APP_EL_FRAME,    // 1px outline, corners cut
    APP_EL_DASH,     // dashed 1px outline (a group)
    APP_EL_DISC,     // filled circle, top-left (x, y), diameter w
    APP_EL_SEL,      // selection: 1px box + 3x3 corner handles
    APP_EL_LABEL,    // canvas label above a frame: glyph `arg` + a name bar `w` long
    APP_EL_LINE,     // (x, y) -> (w, h)
    APP_EL_CLIPBOARD,// 13x17 clipboard at (x, y)
    APP_EL_PLAY,     // play button: disc + black triangle, diameter w
    APP_EL_ROW,      // layers row: y = row index, x = depth, glyph `arg`, name bar `w`, flags `h`
} app_el_op_t;
typedef enum { APP_C_BLACK = 0, APP_C_DARK, APP_C_GREY, APP_C_WHITE, APP_C_AMBER } app_el_color_t;
typedef enum {
    APP_GLYPH_RECT = 0, APP_GLYPH_CIRCLE, APP_GLYPH_FRAME, APP_GLYPH_AUTO, APP_GLYPH_GROUP,
    APP_GLYPH_COMP, APP_GLYPH_INST, APP_GLYPH_TEXT,
} app_glyph_t;
// APP_EL_ROW flags
#define APP_ROW_SEL 0x01      // selected (dark band)
#define APP_ROW_DOT 0x02      // override dot after the name
#define APP_ROW_FIELD 0x04    // inline rename field
#define APP_ROW_ALL 0x08      // name selected (amber, text black)
#define APP_ROW_CURSOR 0x10   // text cursor after the name

typedef struct {
    uint8_t op;    // app_el_op_t
    uint8_t color; // app_el_color_t
    int8_t x, y;
    uint8_t w, h;
    uint8_t arg;
} app_el_t;

typedef struct {
    uint16_t ms;          // how long this keyframe shows
    uint8_t n;
    const app_el_t *el;
} app_keyframe_t;

typedef struct {
    uint8_t n_base;
    const app_el_t *base; // drawn under every keyframe (may be NULL)
    uint8_t n_frames;
    const app_keyframe_t *frames;
} app_scene_t;

// Authoring helpers for scene data (profile files). Colors: K_D dark, K_G grey, K_W white,
// K_A amber, K_B black.
#define K_B APP_C_BLACK
#define K_D APP_C_DARK
#define K_G APP_C_GREY
#define K_W APP_C_WHITE
#define K_A APP_C_AMBER
#define EL_BOX(x, y, w, h, c) {APP_EL_BOX, c, x, y, w, h, 0}
#define EL_FRAME(x, y, w, h, c) {APP_EL_FRAME, c, x, y, w, h, 0}
#define EL_DASH(x, y, w, h, c) {APP_EL_DASH, c, x, y, w, h, 0}
#define EL_DISC(x, y, d, c) {APP_EL_DISC, c, x, y, d, d, 0}
#define EL_SEL(x, y, w, h) {APP_EL_SEL, K_A, x, y, w, h, 0}
#define EL_LABEL(x, y, glyph, len, c) {APP_EL_LABEL, c, x, y, len, 0, glyph}
#define EL_LINE(x0, y0, x1, y1, c) {APP_EL_LINE, c, x0, y0, x1, y1, 0}
#define EL_CLIPBOARD(x, y, c) {APP_EL_CLIPBOARD, c, x, y, 13, 17, 0}
#define EL_PLAY(x, y, d) {APP_EL_PLAY, K_A, x, y, d, d, 0}
#define EL_ROW(i, depth, glyph, len, c, flags) {APP_EL_ROW, c, depth, i, len, flags, glyph}
#define EL_LIST(...) (const app_el_t[]){__VA_ARGS__}
#define EL_COUNT(...) (uint8_t)(sizeof((const app_el_t[]){__VA_ARGS__}) / sizeof(app_el_t))
#define KEYFRAME(ms, ...) {ms, EL_COUNT(__VA_ARGS__), EL_LIST(__VA_ARGS__)}

typedef enum { APP_CMD_KEYS = 0, APP_CMD_ACTIONS } app_cmd_kind_t;

typedef struct {
    const char *name;         // on screen, caps ("WRAP IN FRAME")
    app_cmd_kind_t kind;
    app_key_t key;            // KEYS: the shortcut
    const char *phrase;       // ACTIONS: typed into the app's command search (lowercase ASCII)
    const app_scene_t *scene; // the card, or NULL
} app_cmd_t;

typedef struct {
    const char *name;         // "STRUCTURE"
    const char *tab;          // short name for the ring tabs, <= 6 chars ("BUILD")
    // app_slot_t of the F key that jumps here while the wheel is open. Several rings may share
    // a key: tapping it again steps through them (F1: STRUCTURE -> ALIGN -> STRUCTURE).
    uint8_t slot;
    uint8_t count;
    const app_cmd_t *cmds;    // count entries; "cancel" is added in front by the engine
} app_ring_t;

// ACTIONS: how to reach the app's command search, and how long to wait for it (10ms ticks).
typedef struct {
    app_key_t open;           // Figma: Cmd+K
    uint8_t open_wait;        // after opening, before typing
    uint8_t result_wait;      // after typing, before Enter
} app_search_t;

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
    // Attract screen: the plasma's three hottest steps (RGB888, cool body -> hottest). All 0 =
    // sampled from icon48.
    uint32_t plasma_heat[3];
    app_action_t slot[APP_SLOT_COUNT];
    // Command wheel (a slot with APP_ACT_COMMANDS)
    uint8_t ring_count;
    const app_ring_t *rings;
    app_search_t search;
} app_profile_t;
