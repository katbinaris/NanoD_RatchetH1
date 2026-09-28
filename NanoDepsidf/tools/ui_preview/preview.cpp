// Host preview of the Pixel UI: renders the firmware's own ui_screens / ui_fx code (built
// against the stubs in stub/) into one contact sheet, each screen masked to the round panel
// and shown at 2x. Build and run with tools/ui_preview/run.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "lgfx_config.hpp"
#include "ui_gfx.hpp"
#include "ui_fx.hpp"
#include "ui_screens.hpp"
extern "C" {
#include "ui_state.h"
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
    const ui::Sprite *all[] = {&ui::SPR_USB, &ui::SPR_SPK, &ui::SPR_KBD, &ui::SPR_MOUSE, &ui::SPR_NOTE, &ui::SPR_TERM,
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

    ui::fx_attract(1000);
    keep("plasma 1s");
    ui::fx_attract(3000);
    keep("plasma 3s");
    ui::fx_attract(9000);
    keep("plasma 9s");

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
