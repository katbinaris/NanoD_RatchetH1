#include "ui_cards.hpp"
#include "ui_gfx.hpp"
#include <math.h>
#include <stdlib.h>

namespace ui {

// 7x7 layer-type glyphs (the layers panel's icons, Figma's vocabulary).
static const Sprite GLYPHS[] = {
    {7, 7, ".#####." "#.....#" "#.....#" "#.....#" "#.....#" "#.....#" ".#####."}, // rect
    {7, 7, "..###.." ".#...#." "#.....#" "#.....#" "#.....#" ".#...#." "..###.."}, // circle
    {7, 7, ".#...#." "#######" ".#...#." ".#...#." ".#...#." "#######" ".#...#."}, // frame
    {7, 7, "#.....#" "#.###.#" "#.###.#" "#.###.#" "#.###.#" "#.###.#" "#.....#"}, // auto layout
    {7, 7, "##.#.##" "#.....#" "......." "#.....#" "......." "#.....#" "##.#.##"}, // group
    {7, 7, "...#..." "..###.." "#..#..#" "##...##" "#..#..#" "..###.." "...#..."}, // component
    {7, 7, "...#..." "..#.#.." ".#...#." "#.....#" ".#...#." "..#.#.." "...#..."}, // instance
    {7, 7, "#######" "...#..." "...#..." "...#..." "...#..." "...#..." "...#..."}, // text
};

// macOS modifier glyphs for the shortcut keycaps (the font has none).
static const Sprite G_CTRL = {5, 7, "..#.." ".#.#." "#...#" "....." "....." "....." "....."};
static const Sprite G_OPT = {7, 7, "......." "##..###" "..#...." "...#..." "....#.." ".....##" "......."};
static const Sprite G_SHIFT = {7, 7, "...#..." "..#.#.." ".#...#." "###.###" "..#.#.." "..#.#.." "..###.."};
static const Sprite G_CMD = {7, 7, "##...##" "#.#.#.#" ".#####." "..#.#.." ".#####." "#.#.#.#" "##...##"};

static uint32_t color(uint8_t c) {
    switch (c) {
        case APP_C_DARK: return DARK;
        case APP_C_GREY: return GREY;
        case APP_C_WHITE: return WHITE;
        case APP_C_AMBER: return AMBER;
        default: return BLACK;
    }
}

static void line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        rect(x0, y0, 1, 1, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Dashed 1px outline, 2 on / 2 off, walking the perimeter.
static void dash_box(int x, int y, int w, int h, uint32_t c) {
    int n = 0;
    auto dot = [&](int px, int py) { if ((n++ & 3) < 2) rect(px, py, 1, 1, c); };
    for (int i = 0; i < w; i++) dot(x + i, y);
    for (int j = 1; j < h; j++) dot(x + w - 1, y + j);
    for (int i = w - 2; i >= 0; i--) dot(x + i, y + h - 1);
    for (int j = h - 2; j > 0; j--) dot(x, y + j);
}

// 1px outline with 2px-rounded corners (the card edge).
static void round_frame(int x, int y, int w, int h, uint32_t c) {
    rect(x + 3, y, w - 6, 1, c);
    rect(x + 3, y + h - 1, w - 6, 1, c);
    rect(x, y + 3, 1, h - 6, c);
    rect(x + w - 1, y + 3, 1, h - 6, c);
    const int d[2][2] = {{1, 2}, {2, 1}};
    for (auto &p : d) {
        rect(x + p[0], y + p[1], 1, 1, c);
        rect(x + w - 1 - p[0], y + p[1], 1, 1, c);
        rect(x + p[0], y + h - 1 - p[1], 1, 1, c);
        rect(x + w - 1 - p[0], y + h - 1 - p[1], 1, 1, c);
    }
}

// A layers-panel row. `x0, y0` = the card's top-left.
static void row(const app_el_t &e, int x0, int y0) {
    int ry = y0 + 5 + e.y * 11, rx = x0 + 68 + e.x * 6;
    uint32_t c = color(e.color);
    if (e.h & APP_ROW_SEL) cut(x0 + 66, ry - 2, 51, 11, DARK);
    if (e.arg < sizeof(GLYPHS) / sizeof(GLYPHS[0])) sprite(GLYPHS[e.arg], rx, ry, c);
    int bx = rx + 10;
    if (e.h & APP_ROW_ALL) {
        rect(bx - 1, ry, e.w + 2, 7, AMBER);
        rect(bx, ry + 3, e.w, 2, BLACK);
    } else if (e.w) {
        rect(bx, ry + 3, e.w, 2, c);
    }
    if (e.h & APP_ROW_FIELD) frame_box(bx - 2, ry - 2, x0 + 116 - (bx - 2), 11, AMBER);
    if (e.h & APP_ROW_CURSOR) rect(bx + e.w + 1, ry, 1, 7, AMBER);
    if (e.h & APP_ROW_DOT) cut(bx + e.w + 3, ry + 2, 3, 3, AMBER);
}

static void element(const app_el_t &e, int x0, int y0) {
    if (e.op == APP_EL_ROW) {
        row(e, x0, y0);
        return;
    }
    uint32_t c = color(e.color);
    int x = x0 + e.x, y = y0 + e.y;
    clip(x0 + 4, y0 + 4, 57, 56); // canvas pane
    switch (e.op) {
        case APP_EL_BOX:
            if (e.w >= 4 && e.h >= 4) cut(x, y, e.w, e.h, c); else rect(x, y, e.w, e.h, c);
            break;
        case APP_EL_FRAME: frame_box(x, y, e.w, e.h, c); break;
        case APP_EL_DASH: dash_box(x, y, e.w, e.h, c); break;
        case APP_EL_DISC: disc(x + e.w / 2.0f, y + e.w / 2.0f, e.w / 2.0f, c); break;
        case APP_EL_SEL: {
            rect(x, y, e.w, 1, c);
            rect(x, y + e.h - 1, e.w, 1, c);
            rect(x, y, 1, e.h, c);
            rect(x + e.w - 1, y, 1, e.h, c);
            const int hx[2] = {x - 1, x + e.w - 2}, hy[2] = {y - 1, y + e.h - 2};
            for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) rect(hx[i], hy[j], 3, 3, c);
            break;
        }
        case APP_EL_LABEL:
            if (e.arg < sizeof(GLYPHS) / sizeof(GLYPHS[0])) sprite(GLYPHS[e.arg], x, y, c);
            rect(x + 10, y + 3, e.w, 2, c);
            break;
        case APP_EL_LINE: line(x, y, x0 + (int8_t)e.w, y0 + (int8_t)e.h, c); break;
        case APP_EL_CLIPBOARD:
            frame_box(x, y + 3, 13, 14, c);
            cut(x + 3, y, 7, 5, GREY);
            break;
        case APP_EL_PLAY: {
            float r = e.w / 2.0f;
            disc(x + r, y + r, r, c);
            int th = e.w / 2 | 1, tx = x + (int)r - th / 4 - 1; // odd height, nudged right of centre
            for (int i = 0; i <= th / 2; i++) rect(tx + i, y + (int)r - th / 2 + i, 1, th - 2 * i, BLACK);
            break;
        }
        default: break;
    }
    unclip();
}

void draw_card(const app_scene_t *scene, int x, int y, uint32_t t_ms) {
    round_frame(x, y, CARD_W, CARD_H, DARK);
    if (scene == nullptr) {
        // Cancel: a plain X, nothing to illustrate.
        for (int i = 0; i < 17; i++) {
            rect(x + 52 + i, y + 24 + i, 2, 1, GREY);
            rect(x + 67 - i, y + 24 + i, 2, 1, GREY);
        }
        return;
    }
    rect(x + 63, y + 6, 1, 52, DARK); // canvas | layers
    for (int i = 0; i < scene->n_base; i++) element(scene->base[i], x, y);
    if (scene->n_frames == 0) return;
    uint32_t total = 0;
    for (int i = 0; i < scene->n_frames; i++) total += scene->frames[i].ms;
    uint32_t m = total ? t_ms % total : 0;
    const app_keyframe_t *kf = &scene->frames[scene->n_frames - 1];
    for (int i = 0; i < scene->n_frames; i++) {
        if (m < scene->frames[i].ms) { kf = &scene->frames[i]; break; }
        m -= scene->frames[i].ms;
    }
    for (int i = 0; i < kf->n; i++) element(kf->el[i], x, y);
}

void draw_chord(uint8_t modifier, const char *key, float cx, int y, const char *tail) {
    // macOS order: Control, Option, Shift, Command, then the key.
    const Sprite *mods[4];
    int n = 0;
    if (modifier & (0x01 | 0x10)) mods[n++] = &G_CTRL;
    if (modifier & (0x04 | 0x40)) mods[n++] = &G_OPT;
    if (modifier & (0x02 | 0x20)) mods[n++] = &G_SHIFT;
    if (modifier & (0x08 | 0x80)) mods[n++] = &G_CMD;
    const int cap = 13, gap = 3;
    int kw = key ? text_width(key) + 6 : 0;
    if (key && kw < cap) kw = cap;
    int tw = tail ? 6 + text_width(tail) : 0;
    int total = n * (cap + gap) + kw + tw - (key ? 0 : gap);
    int x = (int)lroundf(cx - total / 2.0f);
    for (int i = 0; i < n; i++) {
        cut(x, y, cap, cap, DARK);
        sprite(*mods[i], x + (cap - mods[i]->w) / 2, y + 3, WHITE);
        x += cap + gap;
    }
    if (key) {
        cut(x, y, kw, cap, DARK);
        text(key, x + kw / 2.0f, y + 3, WHITE, 1, CENTER);
        x += kw;
    }
    if (tail) text(tail, x + 6, y + 3, GREY);
}

void draw_wheel(const WheelView &v) {
    text(v.ring_name, CX, 24, GREY, 1, CENTER);
    rect(60, 40, 120, 1, DARK);

    // The card slides like the PROFILE carousel: the old one leaves, the new one comes in.
    const int CARD_Y = 50, TRAVEL = 128;
    int off = (int)lroundf((1 - v.slide) * v.slide_dir * TRAVEL);
    if (v.prev_valid && v.slide < 1) draw_card(v.prev, CX - CARD_W / 2 + off - v.slide_dir * TRAVEL, CARD_Y, 100000);
    draw_card(v.scene, CX - CARD_W / 2 + off, CARD_Y, v.t_ms);
    if (v.entry > 0) sprite(SPR_TRI_L_M, 22, CARD_Y + 28, AMBER);
    if (v.entry < v.count - 1) sprite(SPR_TRI_R_M, 214, CARD_Y + 28, AMBER);

    text(v.name, CX, 122, WHITE, fit_scale(v.name, 170, 2), CENTER);
    if (v.entry == 0) text("RELEASE TO CLOSE", CX, 148, GREY, 1, CENTER);
    else if (v.search) draw_chord(0x08, "K", CX, 145, "SEARCH");
    else draw_chord(v.modifier, v.key, CX, 145, nullptr);

    int dots_w = v.count * 8 - 4;
    for (int i = 0; i < v.count; i++) rect(lroundf(CX - dots_w / 2.0f) + i * 8, 170, 4, 4, i == v.entry ? AMBER : DARK);
    // Ring tabs: short names, the open ring amber.
    const int TAB_GAP = 9;
    int tabs_w = -TAB_GAP;
    for (int i = 0; i < v.ring_count; i++) tabs_w += text_width(v.ring_tabs[i]) + TAB_GAP;
    float tx = lroundf(CX - tabs_w / 2.0f);
    for (int i = 0; i < v.ring_count; i++) {
        text(v.ring_tabs[i], tx, 188, i == v.ring ? AMBER : DARK);
        tx += text_width(v.ring_tabs[i]) + TAB_GAP;
    }
}

} // namespace ui
