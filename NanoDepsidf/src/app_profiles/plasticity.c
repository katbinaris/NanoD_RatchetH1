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

#define CTRL KEYBOARD_MODIFIER_LEFTCTRL
#define SHIFT KEYBOARD_MODIFIER_LEFTSHIFT
#define OPT KEYBOARD_MODIFIER_LEFTALT

// --- Command wheel (DEVELOPMENT_PLAN.md "Plasticity command wheel", preview round 1, style B
// wireframe: https://claude.ai/artifact/YT2DHdh6CrPHTwZ7KAnZ8s) ---
// Default keys from the Plasticity manual (doc.plasticity.xyz): solid / common commands,
// selection modes, focus & isolate, numpad views; commands without a key go through the
// command palette (F, type, Enter). Cards: an isometric wireframe viewport on the left, the
// selection-mode strip + outliner on the right. Amber = selection / what changes.
#define M_PT 0x01 // selection-mode strip bits: control point, edge, face, solid
#define M_ED 0x02
#define M_FC 0x04
#define M_SO 0x08
#define BOX(f) EL_ISO(32, 30, 16, 16, 12, f)
#define ROW_SEL EL_PROW(0, APP_GLYPH_SOLID, 18, K_W, APP_ROW_SEL)
#define ROWS2(c0, f0, c1, f1) EL_PROW(0, APP_GLYPH_SOLID, 16, c0, f0), EL_PROW(1, APP_GLYPH_SOLID, 12, c1, f1)
#define FLASH EL_FRAME(5, 5, 55, 54, K_A)
#define SCENE(name, ...)                                                           \
    static const app_scene_t name = {                                              \
        .n_frames = (uint8_t)(sizeof((const app_keyframe_t[]){__VA_ARGS__}) / sizeof(app_keyframe_t)), \
        .frames = (const app_keyframe_t[]){__VA_ARGS__},                           \
    }

// SOLID
SCENE(SC_EXTRUDE,
      KEYFRAME(600, EL_ISO(32, 40, 16, 16, 0, APP_ISO_FACE), EL_MODES(M_FC), EL_PROW(0, APP_GLYPH_SHEET, 18, K_W, APP_ROW_SEL)),
      KEYFRAME(80, EL_ISO(32, 38, 16, 16, 5, APP_ISO_FACE), EL_MODES(M_FC), EL_PROW(0, APP_GLYPH_SHEET, 18, K_W, APP_ROW_SEL)),
      KEYFRAME(80, EL_ISO(32, 36, 16, 16, 10, APP_ISO_FACE), EL_MODES(M_FC), EL_PROW(0, APP_GLYPH_SHEET, 18, K_W, APP_ROW_SEL)),
      KEYFRAME(1300, EL_ISO(32, 35, 16, 16, 14, APP_ISO_FACE), EL_MODES(M_FC), EL_PROW(0, APP_GLYPH_SOLID, 18, K_A, 0)));
SCENE(SC_FILLET,
      KEYFRAME(650, BOX(APP_ISO_EDGE), EL_MODES(M_ED), ROW_SEL),
      KEYFRAME(90, EL_ISO_FILLET(32, 30, 16, 16, 12, 3, 0), EL_MODES(M_ED), ROW_SEL),
      KEYFRAME(1300, EL_ISO_FILLET(32, 30, 16, 16, 12, 6, 0), EL_MODES(M_ED), ROW_SEL));
SCENE(SC_BOOLEAN,
      KEYFRAME(650, EL_ISO(30, 38, 20, 20, 8, 0), EL_ISO(30, 20, 10, 10, 8, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_W, APP_ROW_SEL, K_W, APP_ROW_SEL)),
      KEYFRAME(90, EL_ISO(30, 38, 20, 20, 8, 0), EL_ISO(30, 25, 10, 10, 8, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_W, APP_ROW_SEL, K_W, APP_ROW_SEL)),
      KEYFRAME(1300, EL_ISO(30, 38, 20, 20, 8, APP_ISO_SOLID), EL_ISO(30, 30, 10, 10, 8, APP_ISO_SOLID | APP_ISO_NO_BOTTOM), EL_MODES(M_SO),
               EL_PROW(0, APP_GLYPH_SOLID, 16, K_A, 0)));
SCENE(SC_CUT,
      KEYFRAME(650, BOX(0), EL_DLINE(20, 14, 46, 46, K_A), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, EL_ISO(28, 32, 8, 16, 12, 0), EL_ISO(40, 30, 8, 16, 12, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_A, 0, K_A, 0)));
SCENE(SC_OFFSET_FACE,
      KEYFRAME(650, BOX(APP_ISO_FACE), EL_MODES(M_FC), ROW_SEL),
      KEYFRAME(80, EL_ISO(32, 30, 16, 16, 15, APP_ISO_FACE), EL_MODES(M_FC), ROW_SEL),
      KEYFRAME(1300, EL_ISO(32, 30, 16, 16, 18, APP_ISO_FACE), EL_ARROW(52, 26, 52, 14, K_A), EL_MODES(M_FC), ROW_SEL));
SCENE(SC_HOLLOW,
      KEYFRAME(650, BOX(APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, BOX(APP_ISO_HOLLOW), EL_MODES(M_SO), EL_PROW(0, APP_GLYPH_SOLID, 18, K_A, 0)));

// EDIT
SCENE(SC_MOVE,
      KEYFRAME(650, EL_ISO(24, 28, 12, 12, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(90, EL_ISO(24, 28, 12, 12, 10, APP_ISO_GHOST), EL_ISO(31, 31, 12, 12, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, EL_ISO(24, 28, 12, 12, 10, APP_ISO_GHOST), EL_ISO(38, 34, 12, 12, 10, APP_ISO_SOLID),
               EL_ARROW(28, 42, 42, 49, K_A), EL_MODES(M_SO), ROW_SEL));
SCENE(SC_ROTATE,
      KEYFRAME(650, EL_ISO(34, 28, 22, 10, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(90, EL_ISO(32, 28, 16, 16, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, EL_ISO(30, 28, 10, 22, 10, APP_ISO_SOLID), EL_ARC(30, 45, 20, 3, 26, K_A), EL_MODES(M_SO), ROW_SEL));
SCENE(SC_SCALE,
      KEYFRAME(650, EL_ISO(32, 34, 10, 10, 8, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(90, EL_ISO(32, 32, 14, 14, 11, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, EL_ISO(32, 29, 18, 18, 14, APP_ISO_SOLID | APP_ISO_GRIPS), EL_MODES(M_SO), ROW_SEL));
SCENE(SC_DUPLICATE,
      KEYFRAME(650, EL_ISO(24, 26, 12, 12, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(90, EL_ISO(24, 26, 12, 12, 10, 0), EL_ISO(40, 34, 12, 12, 10, APP_ISO_GHOST), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, EL_ISO(24, 26, 12, 12, 10, 0), EL_ISO(40, 34, 12, 12, 10, APP_ISO_SOLID), EL_MODES(M_SO),
               EL_PROW(0, APP_GLYPH_SOLID, 18, K_G, 0), EL_PROW(1, APP_GLYPH_SOLID, 18, K_A, 0)));
SCENE(SC_MIRROR,
      KEYFRAME(650, EL_ISO(22, 26, 10, 12, 12, APP_ISO_SOLID), EL_DLINE(33, 10, 33, 54, K_A), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, EL_ISO(22, 26, 10, 12, 12, 0), EL_DLINE(33, 10, 33, 54, K_A), EL_ISO(46, 26, 12, 10, 12, APP_ISO_SOLID),
               EL_MODES(M_SO), EL_PROW(0, APP_GLYPH_SOLID, 18, K_G, 0), EL_PROW(1, APP_GLYPH_SOLID, 18, K_A, 0)));
SCENE(SC_DELETE,
      KEYFRAME(650, EL_ISO(22, 26, 10, 10, 10, 0), EL_ISO(42, 32, 10, 10, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_G, 0, K_W, APP_ROW_SEL)),
      KEYFRAME(110, EL_ISO(22, 26, 10, 10, 10, 0), EL_ISO(42, 32, 10, 10, 10, APP_ISO_GHOST), EL_MODES(M_SO), ROWS2(K_G, 0, K_D, 0)),
      KEYFRAME(1300, EL_ISO(22, 26, 10, 10, 10, 0), EL_MODES(M_SO), EL_PROW(0, APP_GLYPH_SOLID, 16, K_G, 0)));

// SELECT: the new mode lights up in the strip and on the body.
SCENE(SC_POINTS, KEYFRAME(650, BOX(0), EL_MODES(M_SO), ROW_SEL), KEYFRAME(1300, BOX(APP_ISO_POINTS), EL_MODES(M_PT), ROW_SEL));
SCENE(SC_EDGES, KEYFRAME(650, BOX(0), EL_MODES(M_SO), ROW_SEL), KEYFRAME(1300, BOX(APP_ISO_EDGE), EL_MODES(M_ED), ROW_SEL));
SCENE(SC_FACES, KEYFRAME(650, BOX(0), EL_MODES(M_SO), ROW_SEL), KEYFRAME(1300, BOX(APP_ISO_FACE), EL_MODES(M_FC), ROW_SEL));
SCENE(SC_SOLIDS, KEYFRAME(650, BOX(0), EL_MODES(M_PT), ROW_SEL), KEYFRAME(1300, BOX(APP_ISO_SOLID), EL_MODES(M_SO), ROW_SEL));
SCENE(SC_ALL_TYPES, KEYFRAME(650, BOX(0), EL_MODES(M_SO), ROW_SEL),
      KEYFRAME(1300, BOX(APP_ISO_POINTS), EL_MODES(M_PT | M_ED | M_FC | M_SO), ROW_SEL));
SCENE(SC_SELECT_ALL,
      KEYFRAME(650, EL_ISO(20, 24, 10, 10, 10, 0), EL_ISO(44, 32, 10, 10, 10, 0), EL_MODES(M_SO), ROWS2(K_G, 0, K_G, 0)),
      KEYFRAME(1300, EL_ISO(20, 24, 10, 10, 10, APP_ISO_SOLID), EL_ISO(44, 32, 10, 10, 10, APP_ISO_SOLID), EL_MODES(M_SO),
               ROWS2(K_W, APP_ROW_SEL, K_W, APP_ROW_SEL)));
SCENE(SC_INVERT,
      KEYFRAME(650, EL_ISO(20, 24, 10, 10, 10, APP_ISO_SOLID), EL_ISO(44, 32, 10, 10, 10, 0), EL_MODES(M_SO), ROWS2(K_W, APP_ROW_SEL, K_G, 0)),
      KEYFRAME(1300, EL_ISO(20, 24, 10, 10, 10, 0), EL_ISO(44, 32, 10, 10, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_G, 0, K_W, APP_ROW_SEL)));

// VIEW: the box, a flash as the camera snaps, then the new view.
#define VIEW_SCENE(name, ...) \
    SCENE(name, KEYFRAME(650, BOX(0), EL_MODES(M_SO), ROW_SEL), KEYFRAME(90, BOX(0), FLASH, EL_MODES(M_SO), ROW_SEL), \
          KEYFRAME(1300, __VA_ARGS__, EL_MODES(M_SO), ROW_SEL))
VIEW_SCENE(SC_FRONT, EL_FRAME(16, 20, 32, 24, K_W));
VIEW_SCENE(SC_RIGHT, EL_FRAME(20, 20, 24, 24, K_W));
VIEW_SCENE(SC_TOP, EL_FRAME(16, 16, 32, 32, K_W));
// Perspective: the same box with converging verticals.
VIEW_SCENE(SC_PERSP, EL_LINE(32, 12, 52, 22, K_W), EL_LINE(52, 22, 32, 30, K_W), EL_LINE(32, 30, 12, 22, K_W),
           EL_LINE(12, 22, 32, 12, K_W), EL_LINE(52, 22, 48, 36, K_W), EL_LINE(32, 30, 32, 44, K_W),
           EL_LINE(12, 22, 16, 36, K_W), EL_LINE(48, 36, 32, 44, K_W), EL_LINE(32, 44, 16, 36, K_W));
SCENE(SC_FOCUS,
      KEYFRAME(650, EL_ISO(46, 42, 6, 6, 5, APP_ISO_SOLID), EL_ISO(18, 20, 6, 6, 5, 0), EL_MODES(M_SO), ROWS2(K_G, 0, K_W, APP_ROW_SEL)),
      KEYFRAME(90, EL_ISO(38, 34, 12, 12, 10, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_G, 0, K_W, APP_ROW_SEL)),
      KEYFRAME(1300, EL_ISO(32, 27, 18, 18, 15, APP_ISO_SOLID), EL_MODES(M_SO), ROWS2(K_G, 0, K_W, APP_ROW_SEL)));
#define THREE_ROWS(c0, c2) EL_PROW(0, APP_GLYPH_SOLID, 12, c0, 0), EL_PROW(1, APP_GLYPH_SOLID, 16, K_W, APP_ROW_SEL), \
                           EL_PROW(2, APP_GLYPH_SOLID, 10, c2, 0)
#define THREE_BOXES EL_ISO(18, 22, 8, 8, 8, 0), EL_ISO(34, 28, 10, 10, 10, APP_ISO_SOLID), EL_ISO(50, 36, 8, 8, 8, 0)
SCENE(SC_ISOLATE,
      KEYFRAME(650, THREE_BOXES, EL_MODES(M_SO), THREE_ROWS(K_G, K_G)),
      KEYFRAME(1300, EL_ISO(34, 28, 10, 10, 10, APP_ISO_SOLID), EL_MODES(M_SO), THREE_ROWS(K_D, K_D)));
SCENE(SC_UNISOLATE,
      KEYFRAME(650, EL_ISO(34, 28, 10, 10, 10, APP_ISO_SOLID), EL_MODES(M_SO), THREE_ROWS(K_D, K_D)),
      KEYFRAME(1300, THREE_BOXES, EL_MODES(M_SO), THREE_ROWS(K_A, K_A)));

// Parameter mode (preview v3). In-command keys from the manual: fillet / extrude D = distance
// (the pointer then drives it; fillet: positive = fillet, negative = chamfer). Knob alone =
// fine clicks (`free_step` each, on the FINE haptic profile): smooth enough, and the number never jitters with
// sensor noise the way a continuous value did on hardware; move / rotate /
// scale X / Y / Z = axis, Shift + X / Y / Z = plane, S = uniform. Exact values go in through
// Tab (2026.1: "Commands that share the Gizmo can now use Tab to enter a precise distance
// value"). How free / exact reach the app is to be checked in Plasticity (licence).
#define STEPS_MM {0.05f, 0.10f, 1.00f}
static const app_param_t P_FILLET = {"FILLET", "CHAMFER", STEPS_MM, 0.02f, 8.0f, 0.0f, -5.0f, 5.0f, 2, 0,
                                     APP_PV_FILLET, M_ED, {0, HID_KEY_D}, 0};
static const app_param_t P_EXTRUDE = {"DISTANCE", NULL, STEPS_MM, 0.05f, 8.0f, 0.0f, -8.0f, 10.0f, 2, 0,
                                      APP_PV_EXTRUDE, M_FC, {0, HID_KEY_D}, 0};
static const app_param_t P_OFFSET = {"DISTANCE", NULL, STEPS_MM, 0.02f, 8.0f, 0.0f, -5.0f, 5.0f, 2, 0,
                                     APP_PV_OFFSET, M_FC, {0, 0}, 0};
static const app_param_t P_HOLLOW = {"THICKNESS", NULL, STEPS_MM, 0.02f, 8.0f, 0.5f, 0.05f, 5.0f, 2, 0,
                                     APP_PV_HOLLOW, M_SO, {0, 0}, 0};
static const app_param_t P_MOVE = {"DISTANCE", NULL, STEPS_MM, 0.10f, 16.0f, 0.0f, -20.0f, 20.0f, 2,
                                   APP_PARAM_AXES | APP_PARAM_PLANES, APP_PV_MOVE, M_SO, {0, 0}, 0};
static const app_param_t P_ROTATE = {"ANGLE", NULL, {1.0f, 5.0f, 15.0f}, 1.0f, 16.0f, 0.0f, -360.0f, 360.0f, 0,
                                     APP_PARAM_DEG | APP_PARAM_AXES, APP_PV_ROTATE, M_SO, {0, 0}, 2};
static const app_param_t P_SCALE = {"FACTOR", NULL, STEPS_MM, 0.01f, 8.0f, 1.0f, 0.05f, 5.0f, 2,
                                    APP_PARAM_AXES | APP_PARAM_PLANES | APP_PARAM_UNIFORM, APP_PV_SCALE, M_SO, {0, 0},
                                    APP_AXIS_UNIFORM};

static const app_cmd_t SOLID[] = {
    {"EXTRUDE", APP_CMD_KEYS, {0, HID_KEY_E}, NULL, &SC_EXTRUDE, &P_EXTRUDE, 0},
    {"FILLET", APP_CMD_KEYS, {0, HID_KEY_B}, NULL, &SC_FILLET, &P_FILLET, 0},
    {"BOOLEAN", APP_CMD_KEYS, {0, HID_KEY_Q}, NULL, &SC_BOOLEAN, NULL, 0},
    {"CUT", APP_CMD_KEYS, {0, HID_KEY_C}, NULL, &SC_CUT, NULL, 0},
    {"OFFSET FACE", APP_CMD_ACTIONS, {0, 0}, "offset face", &SC_OFFSET_FACE, &P_OFFSET, 0},
    {"HOLLOW", APP_CMD_ACTIONS, {0, 0}, "hollow", &SC_HOLLOW, &P_HOLLOW, 0},
};
static const app_cmd_t EDIT[] = {
    {"MOVE", APP_CMD_KEYS, {0, HID_KEY_G}, NULL, &SC_MOVE, &P_MOVE, 0},
    {"ROTATE", APP_CMD_KEYS, {0, HID_KEY_R}, NULL, &SC_ROTATE, &P_ROTATE, 0},
    {"SCALE", APP_CMD_KEYS, {0, HID_KEY_S}, NULL, &SC_SCALE, &P_SCALE, 0},
    {"DUPLICATE", APP_CMD_KEYS, {SHIFT, HID_KEY_D}, NULL, &SC_DUPLICATE, NULL, 0},
    {"MIRROR", APP_CMD_KEYS, {OPT, HID_KEY_X}, NULL, &SC_MIRROR, NULL, 0},
    {"DELETE", APP_CMD_KEYS, {SHIFT, HID_KEY_X}, NULL, &SC_DELETE, NULL, 0},
};
static const app_cmd_t SELECT[] = {
    {"CONTROL POINTS", APP_CMD_KEYS, {0, HID_KEY_1}, NULL, &SC_POINTS, NULL, 0},
    {"EDGES", APP_CMD_KEYS, {0, HID_KEY_2}, NULL, &SC_EDGES, NULL, 0},
    {"FACES", APP_CMD_KEYS, {0, HID_KEY_3}, NULL, &SC_FACES, NULL, 0},
    {"SOLIDS", APP_CMD_KEYS, {0, HID_KEY_4}, NULL, &SC_SOLIDS, NULL, 0},
    {"ALL TYPES", APP_CMD_KEYS, {0, HID_KEY_TAB}, NULL, &SC_ALL_TYPES, NULL, 0},
    {"SELECT ALL", APP_CMD_KEYS, {0, HID_KEY_A}, NULL, &SC_SELECT_ALL, NULL, 0},
    {"INVERT", APP_CMD_KEYS, {OPT, HID_KEY_A}, NULL, &SC_INVERT, NULL, 0},
};
// Numpad keys, sent as keypad usages -- a Mac keyboard without a numpad doesn't matter.
static const app_cmd_t VIEW[] = {
    {"FRONT", APP_CMD_KEYS, {0, HID_KEY_KEYPAD_1}, NULL, &SC_FRONT, NULL, 0},
    {"RIGHT", APP_CMD_KEYS, {0, HID_KEY_KEYPAD_3}, NULL, &SC_RIGHT, NULL, 0},
    {"TOP", APP_CMD_KEYS, {0, HID_KEY_KEYPAD_7}, NULL, &SC_TOP, NULL, 0},
    {"PERSPECTIVE", APP_CMD_KEYS, {0, HID_KEY_KEYPAD_5}, NULL, &SC_PERSP, NULL, 0},
    {"FOCUS", APP_CMD_KEYS, {0, HID_KEY_SLASH}, NULL, &SC_FOCUS, NULL, 0},
    {"ISOLATE", APP_CMD_KEYS, {0, HID_KEY_PERIOD}, NULL, &SC_ISOLATE, NULL, 0},
    {"UNISOLATE", APP_CMD_KEYS, {OPT, HID_KEY_PERIOD}, NULL, &SC_UNISOLATE, NULL, 0},
};
// F1 steps SOLID <-> EDIT (modelling), F2 SELECT, F4 VIEW.
static const app_ring_t RINGS[] = {
    {"SOLID", "SOLID", APP_SLOT_F1, 6, SOLID},
    {"EDIT", "EDIT", APP_SLOT_F1, 6, EDIT},
    {"SELECT", "SELECT", APP_SLOT_F2, 7, SELECT},
    {"VIEW", "VIEW", APP_SLOT_F4, 7, VIEW},
};

const app_profile_t app_profile_plasticity = {
    .version = APP_PROFILE_VERSION,
    .id = "plasticity",
    .name = "PLASTICITY",
    .icon24 = app_icon_plasticity_24,
    .icon48 = app_icon_plasticity_48,
    .legend = {"ZOOM", "ORBIT", "WHEEL", "PAN"},
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
        // Hold = the command wheel; a quick tap is still Undo, Cmd+Z on macOS (HID Left GUI =
        // Cmd; KEYBOARD_MODIFIER_LEFTCTRL on Windows) -- the shape flashes for it.
        [APP_SLOT_F3] = {
            .kind = APP_ACT_COMMANDS, .label = "UNDO",
            .tap = {KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_Z}, .fx = APP_FX_FLASH,
            .feel = HAPTIC_TYPE_SAW, .detents = 12,
        },
        [APP_SLOT_F4] = {
            .kind = APP_ACT_DRAG, .label = "PAN",
            .buttons = MOUSE_BUTTON_RIGHT, .px_per_rad = PX_PER_RAD, .sign = 1,
            .feel = HAPTIC_TYPE_VISCOSE, .fx = APP_FX_PAN,
        },
    },
    .ring_count = 4,
    .rings = RINGS,
    .search = {{0, HID_KEY_F}, 20, 30}, // the command palette: F, type, Enter
    .param_keys = {
        .numeric = {0, HID_KEY_TAB}, .confirm = {0, HID_KEY_ENTER}, .cancel = {0, HID_KEY_ESCAPE},
        .axis = {{0, HID_KEY_X}, {0, HID_KEY_Y}, {0, HID_KEY_Z}}, .uniform = {0, HID_KEY_S},
    },
};
