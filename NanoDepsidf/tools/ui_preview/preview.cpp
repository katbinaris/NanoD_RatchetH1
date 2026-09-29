// Host preview of the Pixel UI: renders the firmware's own ui_screens / ui_fx code (built
// against the stubs in stub/) into one contact sheet, each screen masked to the round panel
// and shown at 2x. Build and run with tools/ui_preview/run.sh.
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "lgfx_config.hpp"
#include "ui_gfx.hpp"
#include "ui_fx.hpp"
#include "ui_screens.hpp"
#include "icons/app_icons.h"
extern "C" {
#include "ui_state.h"
#include "app_profiles/app_profile.h"
extern const app_profile_t app_profile_figma;
}

static LGFX_Sprite g;

struct Tile {
    const char *name;
    std::vector<uint32_t> px;
};
static std::vector<Tile> tiles;

static void keep(const char *name) {
    tiles.push_back({name, std::vector<uint32_t>(g.px, g.px + 240 * 240)});
    g.clear();
}

static menu_render_row_t row(const char *label, const char *caption, const char *value, bool sel) {
    menu_render_row_t r = {};
    snprintf(r.label, sizeof(r.label), "%s", label);
    snprintf(r.caption, sizeof(r.caption), "%s", caption);
    snprintf(r.value, sizeof(r.value), "%s", value);
    r.selected = sel;
    return r;
}

static menu_render_snapshot_t haptic_snap(int selected, bool editing, bool dirty) {
    menu_render_snapshot_t s = {};
    s.open = true;
    s.screen = MENU_SCREEN_HAPTIC;
    s.editing = editing;
    s.dirty = dirty;
    s.selected = selected;
    const char *L[6][3] = {{"STEPS", "DETENTS", "12"}, {"SNAP", "KP", "6.00"}, {"DAMP", "KD", ".010"},
                           {"FEEL", "TYPE", "VISCOSE"}, {"TONE", "CLICK", "WOOD"}, {"PITCH", "CLICK", "1.00X"}};
    for (int i = 0; i < 6; i++) s.rows[i] = row(L[i][0], L[i][1], L[i][2], i == selected);
    s.row_count = 6;
    return s;
}

int main() {
    ui::bind(&g);
    ui::fx_init();

    // Hand-drawn sprites: the row string must be exactly w*h chars.
    const ui::Sprite *all[] = {&ui::SPR_USB, &ui::SPR_SPK, &ui::SPR_KBD, &ui::SPR_MOUSE, &ui::SPR_NOTE, &ui::SPR_TERM, &ui::SPR_CUBE, &ui::SPR_CUBE_M,
                               &ui::SPR_TRI_L, &ui::SPR_TRI_R, &ui::SPR_STEPS, &ui::SPR_SNAP, &ui::SPR_DAMP,
                               &ui::SPR_PITCH, &ui::SPR_USB_M, &ui::SPR_SPK_M, &ui::SPR_KBD_M, &ui::SPR_MOUSE_M,
                               &ui::SPR_NOTE_M, &ui::SPR_TERM_M, &ui::SPR_TRI_L_M, &ui::SPR_TRI_R_M,
                               &ui::SPR_STEPS_M, &ui::SPR_SNAP_M, &ui::SPR_DAMP_M, &ui::SPR_PITCH_M};
    for (const ui::Sprite *s : all) {
        if (strlen(s->rows) != (size_t)s->w * s->h) {
            fprintf(stderr, "sprite %dx%d has %zu chars\n", s->w, s->h, strlen(s->rows));
            return 1;
        }
    }

    for (uint32_t e : {300u, 800u, 1400u, 2600u}) {
        ui::fx_boot(e);
        static char n[32];
        keep(strdup((snprintf(n, sizeof(n), "boot %ums", e), n)));
    }

    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_MOUSE, HAPTIC_TYPE_SINE, 0, nullptr});
    keep("main mouse");
    ui::draw_main({true, AUDIO_TIMBRE_TICK_THUD, MENU_HID_KEYBOARD, HAPTIC_TYPE_VISCOSE, UI_BTN_F4, nullptr});
    keep("main keyboard, F4 held");
    ui::AppView figma = {"FIGMA", app_icon_figma_24, {"UNDO", "LAYER", "FOCUS", "NUDGE"}, "ZOOM", "KNOB"};
    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, 0, nullptr, &figma});
    keep("main APP figma");
    ui::AppView figma_f1 = figma;
    figma_f1.action = "UNDO/REDO";
    figma_f1.action_via = "F1 + KNOB";
    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, UI_BTN_F1, nullptr, &figma_f1});
    keep("main APP figma, F1 held");
    // Plasticity's micro-interaction: an isometric pyramid in each scene.
    ui::ShapeView sv = {ui::SHAPE_PYRAMID, ui::STYLE_THICK, ui::SCENE_ZOOM, (float)M_PI / 4, 0.375f, 0, false};
    ui::AppView plast = {"PLASTICITY", app_icon_plasticity_24, {"ZOOM", "ORBIT", "UNDO", "PAN"}, "ZOOM", "KNOB", "KNOB", &sv, false};
    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, 0, nullptr, &plast});
    keep("plasticity ZOOM");
    sv.scene = ui::SCENE_ORBIT;
    plast.action = "ORBIT";
    plast.action_key = "F2";
    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, UI_BTN_F2, nullptr, &plast});
    keep("plasticity ORBIT at rest (45 deg)");
    sv.yaw = (float)M_PI / 4 + 2 * (float)M_PI / 32 * 3; // a stepped pose
    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, UI_BTN_F2, nullptr, &plast});
    keep("plasticity ORBIT turning");
    sv.yaw = (float)M_PI / 4;
    sv.scene = ui::SCENE_PAN;
    sv.pan = 20;
    plast.action = "PAN";
    plast.action_key = "F4";
    ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, UI_BTN_F4, nullptr, &plast});
    keep("plasticity PAN");
    // Uploaded icon: raw RGB565 BE written by run.sh from tools/icons/figma_pixel_48.png.
    if (FILE *f = fopen(getenv("ICON_RAW") ? getenv("ICON_RAW") : "", "rb")) {
        static uint8_t icon[48 * 48 * 2];
        size_t n = fread(icon, 1, sizeof(icon), f);
        fclose(f);
        if (n == sizeof(icon)) {
            ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_MOUSE, HAPTIC_TYPE_SINE, 0, icon});
            keep("main + uploaded icon");
        }
    }

    menu_render_snapshot_t root = {};
    root.open = true;
    root.screen = MENU_SCREEN_ROOT;
    root.selected = 1;
    root.rows[0] = row("HAPTICS", "", "", false);
    root.rows[1] = row("HID TYPE", "", "", true);
    root.rows[2] = row("BOOT MODE", "", "", false);
    root.row_count = 3;
    ui::draw_menu_list(root, 32);
    keep("menu");

    ui::draw_orbit(haptic_snap(MENU_HAPTIC_ROW_FEEL, true, true), {HAPTIC_TYPE_SINE, 700, -1, 1, true});
    keep("orbit FEEL editing");
    ui::draw_orbit(haptic_snap(MENU_HAPTIC_ROW_SNAP, true, false), {HAPTIC_TYPE_SAW, 0, -1, 1, true});
    keep("orbit SNAP editing");
    ui::draw_orbit(haptic_snap(MENU_HAPTIC_ROW_STEPS, false, false), {HAPTIC_TYPE_SAW, 0, -1, 1, true});
    ui::draw_saved_toast();
    keep("orbit + SAVED!");

    menu_render_snapshot_t hid = {};
    hid.open = true;
    hid.screen = MENU_SCREEN_HID;
    hid.selected = 0;
    hid.rows[0] = row("HID TYPE", "", "KEYBOARD", true);
    hid.row_count = 1;
    ui::draw_hid(hid, {MENU_HID_KEYBOARD, 0, true});
    keep("hid keyboard");
    hid.rows[0] = row("HID TYPE", "", "APP", true);
    ui::draw_hid(hid, {MENU_HID_APP, 0, true, "FIGMA", app_icon_figma_24});
    keep("hid APP");

    static const ui::ProfileItem profiles[2] = {
        {"PLASTICITY", app_icon_plasticity_24, app_icon_plasticity_48, {"ZOOM", "ORBIT", "UNDO", "PAN"}},
        {"FIGMA", app_icon_figma_24, app_icon_figma_48, {"UNDO", "LAYER", "FOCUS", "NUDGE"}},
    };
    menu_render_snapshot_t prof = {};
    prof.open = true;
    prof.screen = MENU_SCREEN_APP_PROFILE;
    prof.rows[0] = row("PROFILE", "", "FIGMA", true);
    prof.row_count = 1;
    ui::draw_app_profile(prof, {profiles, 2, 0, 0, true});
    keep("profile PLASTICITY, saved");
    prof.dirty = true;
    ui::draw_app_profile(prof, {profiles, 2, 1, 0, true});
    keep("profile FIGMA, unsaved");
    hid.selected = 1;
    hid.dirty = true;
    hid.rows[0] = row("HID TYPE", "", "MIDI", false);
    hid.rows[1] = row("CHANNEL", "", "01", true);
    hid.row_count = 2;
    ui::draw_hid(hid, {MENU_HID_MIDI, 0, true});
    keep("hid MIDI channel");

    menu_render_snapshot_t boot = {};
    boot.open = true;
    boot.screen = MENU_SCREEN_BOOT;
    boot.selected = 0;
    boot.dirty = true;
    boot.rows[0] = row("USB MODE", "", "SERIAL", true);
    boot.row_count = 1;
    ui::draw_boot_mode(boot, BOOT_USB_MODE_SERIAL, false, true);
    keep("boot mode");

    // Command wheel (Figma): each card on its resting (last) keyframe, one mid-animation,
    // cancel, a slide in flight, and the Main Screen echo after a run.
    {
        const app_profile_t &p = app_profile_figma;
        auto rest_ms = [](const app_scene_t *s) {
            uint32_t t = 0;
            for (int i = 0; i + 1 < s->n_frames; i++) t += s->frames[i].ms;
            return t;
        };
        auto wheel = [&](int ring, int entry, uint32_t t, float slide, const app_scene_t *prev) {
            static const char *const KEYS[5] = {"", "F1", "F2", "F3", "F4"};
            static char key[2];
            const app_ring_t &r = p.rings[ring];
            ui::WheelView v = {};
            v.ring_name = r.name;
            v.ring_count = p.ring_count;
            for (int i = 0; i < p.ring_count; i++) v.ring_keys[i] = KEYS[p.rings[i].slot];
            v.ring = ring;
            v.count = r.count + 1;
            v.entry = entry;
            const app_cmd_t *c = entry ? &r.cmds[entry - 1] : nullptr;
            v.name = c ? c->name : "CANCEL";
            v.scene = c ? c->scene : nullptr;
            if (c && c->kind == APP_CMD_ACTIONS) v.search = true;
            else if (c) {
                v.modifier = c->key.modifier;
                key[0] = (char)('A' + c->key.keycode - 0x04);
                key[1] = 0;
                v.key = key;
            }
            v.slide = slide;
            v.slide_dir = 1;
            v.prev = prev;
            v.prev_valid = slide < 1;
            v.t_ms = c && t == UINT32_MAX ? rest_ms(c->scene) : t;
            ui::MainInputs in = {false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, UI_BTN_F3, nullptr, nullptr, &v};
            ui::draw_main(in);
        };
        for (int ring = 0; ring < p.ring_count; ring++)
            for (int e = 1; e <= p.rings[ring].count; e++) {
                wheel(ring, e, UINT32_MAX, 1, nullptr);
                keep(p.rings[ring].cmds[e - 1].name);
            }
        wheel(0, 1, 0, 1, nullptr);
        keep("ADD AUTO LAYOUT, start");
        wheel(0, 0, 0, 1, nullptr);
        keep("wheel cancel");
        wheel(0, 3, UINT32_MAX, 0.4f, p.rings[0].cmds[1].scene);
        keep("wheel sliding");
        ui::AppView echo = {"FIGMA", app_icon_figma_24, {"UNDO", "DEPTH", "WHEEL", "FRAME"}, "ZOOM", "KNOB"};
        echo.echo = true;
        echo.echo_scene = p.rings[0].cmds[2].scene;
        echo.echo_name = p.rings[0].cmds[2].name;
        echo.echo_ms = rest_ms(echo.echo_scene);
        ui::draw_main({false, AUDIO_TIMBRE_WOOD_TOCK, MENU_HID_APP, HAPTIC_TYPE_SAW, 0, nullptr, &echo});
        keep("echo after run");
    }

    menu_render_snapshot_t disp = {};
    disp.open = true;
    disp.screen = MENU_SCREEN_DISPLAY;
    disp.dirty = true;
    disp.row_count = 1;
    ui::draw_display(disp, 1, true);
    keep("display rotation 90, unsaved");

    ui::fx_attract(1000);
    keep("plasma 1s");
    ui::fx_attract(3000);
    keep("plasma 3s");
    ui::fx_attract(9000);
    keep("plasma 9s");
    for (uint32_t ms : {1000u, 3000u, 5500u}) {
        ui::fx_attract(ms, app_icon_figma_48, app_profile_figma.plasma_heat);
        keep("plasma figma");
    }
    ui::fx_attract(6000, app_icon_plasticity_48);
    keep("plasma plasticity 6s");

    // Contact sheet: 4 per row, 2x, round mask, 8px gutters. Binary PPM on stdout; names on stderr.
    const int cols = 4, sc = 2, cell = 240 * sc + 16;
    int rows = (int)(tiles.size() + cols - 1) / cols;
    int W = cols * cell, H = rows * cell;
    std::vector<uint8_t> img(W * H * 3, 24);
    for (size_t t = 0; t < tiles.size(); t++) {
        int ox = (int)(t % cols) * cell + 8, oy = (int)(t / cols) * cell + 8;
        for (int y = 0; y < 240 * sc; y++)
            for (int x = 0; x < 240 * sc; x++) {
                float dx = x / (float)sc - 119.5f, dy = y / (float)sc - 119.5f;
                uint32_t c = (dx * dx + dy * dy <= 120 * 120) ? tiles[t].px[(y / sc) * 240 + x / sc] : 0x181818;
                uint8_t *p = &img[((oy + y) * W + ox + x) * 3];
                p[0] = c >> 16; p[1] = c >> 8; p[2] = c;
            }
        fprintf(stderr, "%zu: %s\n", t, tiles[t].name);
    }
    printf("P6\n%d %d\n255\n", W, H);
    fwrite(img.data(), 1, img.size(), stdout);
    return 0;
}
