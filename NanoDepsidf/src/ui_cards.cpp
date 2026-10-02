#include "ui_cards.hpp"
#include "ui_gfx.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    {7, 7, "..###.." ".#...#." "#######" "#.....#" "#.....#" "#.....#" "#######"}, // solid (Plasticity)
    {7, 7, "......." "..#####" ".#...#." "#####.." "......." "......." "......."}, // sheet / plane
    {7, 7, ".....##" "....#.#" "...#.#." "..#.#.." ".#.#..." "##....." "#......"}, // sketch (Onshape)
};

// Plasticity's selection modes: control point, edge, face, solid (5x5, in 11x9 chips).
static const Sprite MODE_GLYPHS[4] = {
    {5, 5, "....." ".###." ".###." ".###." "....."},
    {5, 5, "....#" "...#." "..#.." ".#..." "#...."},
    {5, 5, "..###" ".####" "#####" "####." "###.."},
    {5, 5, ".###." "#####" "#####" "#####" ".###."},
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

// pattern: 0 solid, 1 dotted (1 on 1 off), 2 dashed (2 on 2 off)
static void line(int x0, int y0, int x1, int y1, uint32_t c, int pattern = 0) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int n = 0;; n++) {
        if (pattern == 0 || (pattern == 1 ? (n & 1) == 0 : (n & 3) < 2)) rect(x0, y0, 1, 1, c);
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

// Convex polygon, filled at pixel centres; `dots` = every other pixel (a selected face in
// the wireframe, lighter than solid).
static void poly(const int (*p)[2], int n, uint32_t c, bool dots) {
    int y0 = p[0][1], y1 = p[0][1];
    for (int i = 1; i < n; i++) { if (p[i][1] < y0) y0 = p[i][1]; if (p[i][1] > y1) y1 = p[i][1]; }
    for (int y = y0; y <= y1; y++) {
        float py = y + 0.5f, lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < n; i++) {
            float ax = p[i][0], ay = p[i][1], bx = p[(i + 1) % n][0], by = p[(i + 1) % n][1];
            if ((ay <= py && by > py) || (by <= py && ay > py)) {
                float x = ax + (py - ay) / (by - ay) * (bx - ax);
                if (x < lo) lo = x;
                if (x > hi) hi = x;
            }
        }
        if (lo > hi) continue;
        for (int x = (int)ceilf(lo - 0.5f); x <= (int)floorf(hi - 0.5f); x++) {
            if (dots && ((x + y) & 1)) continue;
            rect(x, y, 1, 1, c);
        }
    }
}

// Isometric 2:1 wireframe box (APP_EL_ISO). Viewed from +x +y +z: the top, x = a and y = b
// faces show; the three edges meeting at (0,0,0) are hidden (dotted dark).
static void iso_box(const app_el_t &e, int x0, int y0) {
    const int ox = x0 + e.x, oy = y0 + e.y, a = e.w, b = e.h, h = e.d, f = e.flags;
    const int r = (f & APP_ISO_HOLLOW) ? 0 : (e.arg & 0x3F); // arg bits 6-7: APP_ISO_ARG_*
    const bool chamfer = e.arg & APP_ISO_ARG_CHAMFER;
    auto P = [&](int X, int Y, int Z, int *o) { o[0] = ox + X - Y; o[1] = oy + ((X + Y + 1) >> 1) - Z; };
    auto E = [&](int X0, int Y0, int Z0, int X1, int Y1, int Z1, uint32_t c, int pat) {
        int p0[2], p1[2];
        P(X0, Y0, Z0, p0);
        P(X1, Y1, Z1, p1);
        line(p0[0], p0[1], p1[0], p1[1], c, pat);
    };
    const bool ghost = f & APP_ISO_GHOST;
    const uint32_t edge = ghost ? DARK : (f & APP_ISO_SOLID) ? AMBER : color(e.color);
    const int pat = ghost ? 2 : 0;
    if (!ghost && (e.arg & APP_ISO_ARG_CUT) && !(f & APP_ISO_HOLLOW)) { // a section cut: the x = a face
        int s[4][2];
        P(a, 0, 0, s[0]); P(a, b, 0, s[1]); P(a, b, h, s[2]); P(a, 0, h, s[3]);
        poly(s, 4, AMBER, true);
    }
    if (!ghost) {
        E(0, 0, 0, a, 0, 0, DARK, 1);
        E(0, 0, 0, 0, b, 0, DARK, 1);
        if (h > 0) E(0, 0, 0, 0, 0, h, DARK, 1);
    }
    int top[5][2];
    int nt = 0;
    P(0, 0, h, top[nt++]);
    P(a, 0, h, top[nt++]);
    if (r) { P(a, b - r, h, top[nt++]); P(a - r, b, h, top[nt++]); } else P(a, b, h, top[nt++]);
    P(0, b, h, top[nt++]);
    if (r && !ghost) {
        int s[4][2];
        P(a, b - r, 0, s[0]); P(a - r, b, 0, s[1]); P(a - r, b, h, s[2]); P(a, b - r, h, s[3]);
        if (chamfer) for (int i = 0; i < 4; i++) line(s[i][0], s[i][1], s[(i + 1) % 4][0], s[(i + 1) % 4][1], AMBER);
        else poly(s, 4, AMBER, true);
    }
    const bool face = f & APP_ISO_FACE;
    if (face && !ghost) poly(top, nt, AMBER, true);
    for (int i = 0; i < nt; i++) {
        const int *p0 = top[i], *p1 = top[(i + 1) % nt];
        line(p0[0], p0[1], p1[0], p1[1], face ? AMBER : edge, pat);
    }
    if (h > 0) {
        E(a, 0, 0, a, 0, h, edge, pat);
        E(0, b, 0, 0, b, h, edge, pat);
        if (!(f & APP_ISO_NO_BOTTOM)) {
            E(a, 0, 0, a, b - r, 0, edge, pat);
            E(0, b, 0, a - r, b, 0, edge, pat);
        }
        if (r) {
            E(a, b - r, 0, a, b - r, h, edge, pat);
            E(a - r, b, 0, a - r, b, h, edge, pat);
            if (!(f & APP_ISO_NO_BOTTOM)) E(a, b - r, 0, a - r, b, 0, edge, pat);
        } else {
            bool sel = f & APP_ISO_EDGE;
            E(a, b, 0, a, b, h, sel ? AMBER : edge, pat);
            if (sel) {
                int p0[2], p1[2];
                P(a, b, 0, p0);
                P(a, b, h, p1);
                line(p0[0] + 1, p0[1], p1[0] + 1, p1[1], AMBER);
            }
        }
    }
    if (f & APP_ISO_HOLLOW) {
        const int in = r ? r : 3; // HOLLOW: `arg` is the rim inset (no fillet then)
        int q[4][2];
        P(in, in, h, q[0]); P(a - in, in, h, q[1]); P(a - in, b - in, h, q[2]); P(in, b - in, h, q[3]);
        for (int i = 0; i < 4; i++) line(q[i][0], q[i][1], q[(i + 1) % 4][0], q[(i + 1) % 4][1], AMBER);
        E(in, in, h, in, in, h > 6 ? h - 6 : 0, DARK, 0);
    }
    if (f & APP_ISO_POINTS) {
        const int pts[7][3] = {{0, 0, h}, {a, 0, h}, {a, b, h}, {0, b, h}, {a, 0, 0}, {a, b, 0}, {0, b, 0}};
        for (auto &q : pts) { int p[2]; P(q[0], q[1], q[2], p); rect(p[0] - 1, p[1] - 1, 3, 3, AMBER); }
    }
    if (f & APP_ISO_GRIPS) {
        const int pts[4][3] = {{0, 0, h}, {a, 0, h}, {a, b, 0}, {0, b, h}};
        for (auto &q : pts) {
            int p[2];
            P(q[0], q[1], q[2], p);
            rect(p[0] - 2, p[1] - 2, 5, 5, AMBER);
            rect(p[0] - 1, p[1] - 1, 3, 3, BLACK);
        }
    }
}

// Plasticity's selection-mode strip, top of the right pane; `bits` = active modes.
static void modes(uint8_t bits, int x0, int y0) {
    for (int i = 0; i < 4; i++) {
        bool on = bits & (1 << i);
        int x = x0 + 67 + i * 13;
        cut(x, y0 + 5, 11, 9, on ? AMBER : DARK);
        sprite(MODE_GLYPHS[i], x + 3, y0 + 7, on ? BLACK : GREY);
    }
}

// Arc (APP_EL_ARC): an iso ellipse (rotate, revolve) or, with APP_ARC_ROUND, a circle (a sketch);
// a 3x3 head at the end unless APP_ARC_NO_HEAD. Dotted: every other point along the walk.
static void iso_arc(int cx, int cy, int r, float t0, float t1, uint32_t c, uint8_t flags = 0) {
    const bool round = flags & APP_ARC_ROUND;
    int lx = 0, ly = 0;
    bool have = false;
    float step = t1 > t0 ? 0.04f : -0.04f;
    for (float t = t0; step > 0 ? t <= t1 + 1e-4f : t >= t1 - 1e-4f; t += step) {
        int x = (int)lroundf(cx + r * cosf(t));
        int y = (int)lroundf(round ? cy - r * sinf(t) : cy + r / 2.0f * sinf(t));
        if (have && (x != lx || y != ly)) line(lx, ly, x, y, c, (flags & APP_ARC_DOTTED) ? 1 : 0);
        lx = x;
        ly = y;
        have = true;
    }
    if (have && !(flags & APP_ARC_NO_HEAD)) rect(lx - 1, ly - 1, 3, 3, c);
}

// A layers-panel row. `x0, y0` = the card's top-left.
static void row(const app_el_t &e, int x0, int y0) {
    int ry = y0 + 5 + e.d + e.y * 11, rx = x0 + 68 + e.x * 6;
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
    if (e.op == APP_EL_MODES) {
        modes(e.arg, x0, y0);
        return;
    }
    if (e.op == APP_EL_ROLLBACK) { // a grey rule with a grip, just under the previous row
        int ry = y0 + 5 + e.d + e.y * 11 - 2;
        rect(x0 + 67, ry, 49, 1, GREY);
        rect(x0 + 67, ry - 1, 3, 3, GREY);
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
        case APP_EL_LINE: {
            int x1 = x0 + (int8_t)e.w, y1 = y0 + (int8_t)e.h;
            line(x, y, x1, y1, c, (e.arg & APP_LINE_DASHED) ? 2 : 0);
            if (e.arg & APP_LINE_HEAD) rect(x1 - 1, y1 - 1, 3, 3, c);
            break;
        }
        case APP_EL_ISO: iso_box(e, x0, y0); break;
        case APP_EL_ARC: iso_arc(x, y, e.w, e.arg / 10.0f, e.d / 10.0f, c, e.flags); break;
        case APP_EL_NUM: {
            char n[4];
            snprintf(n, sizeof(n), "%u", e.arg);
            text(n, x, y, c, 1, CENTER);
            break;
        }
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

// --- parameter mode ---

static app_el_t iso_el(int x, int y, int a, int b, int h, uint8_t flags, uint8_t arg = 0) {
    app_el_t e = {};
    e.op = APP_EL_ISO;
    e.color = APP_C_WHITE;
    e.x = (int8_t)x; e.y = (int8_t)y;
    e.w = (uint8_t)a; e.h = (uint8_t)b; e.d = (uint8_t)(h < 0 ? 0 : h);
    e.arg = arg; e.flags = flags;
    return e;
}

// A box from its 8 corners so it can move / rotate / scale on any axis. Extents x +-a/2,
// y +-b/2, z 0..h; transforms about its centre. Farthest corner's edges dotted dark.
struct Box3 {
    float scale[3] = {1, 1, 1}, move[3] = {0, 0, 0};
    int axis = 2;
    float angle = 0;
    bool ghost = false, grips = false;
};
static void box3(int ox, int oy, float a, float b, float h, const Box3 &o) {
    float pts[8][3];
    const float c = cosf(o.angle), s = sinf(o.angle), zc = h / 2;
    for (int k = 0; k < 8; k++) {
        float x = ((k & 1) ? 1 : -1) * a * o.scale[0] / 2, y = ((k & 2) ? 1 : -1) * b * o.scale[1] / 2,
              z = ((k & 4) ? 1 : -1) * h * o.scale[2] / 2;
        float t;
        if (o.axis == 0) { t = y * c - z * s; z = y * s + z * c; y = t; }
        else if (o.axis == 1) { t = x * c + z * s; z = -x * s + z * c; x = t; }
        else { t = x * c - y * s; y = x * s + y * c; x = t; }
        pts[k][0] = x + o.move[0];
        pts[k][1] = y + o.move[1];
        pts[k][2] = z + zc + o.move[2];
    }
    int far = 0;
    for (int k = 1; k < 8; k++) {
        if (pts[k][0] + pts[k][1] + 1.2f * pts[k][2] < pts[far][0] + pts[far][1] + 1.2f * pts[far][2]) far = k;
    }
    int sp[8][2];
    for (int k = 0; k < 8; k++) {
        sp[k][0] = ox + (int)lroundf(pts[k][0] - pts[k][1]);
        sp[k][1] = oy + (int)lroundf((pts[k][0] + pts[k][1]) / 2 - pts[k][2]);
    }
    for (int i = 0; i < 8; i++) {
        for (int bit = 1; bit <= 4; bit <<= 1) {
            int j = i | bit;
            if (j == i) continue;
            bool hidden = i == far || j == far;
            if (o.ghost) line(sp[i][0], sp[i][1], sp[j][0], sp[j][1], DARK, 2);
            else line(sp[i][0], sp[i][1], sp[j][0], sp[j][1], hidden ? DARK : AMBER, hidden ? 1 : 0);
        }
    }
    if (o.grips && !o.ghost) {
        for (int k = 4; k < 8; k++) {
            rect(sp[k][0] - 2, sp[k][1] - 2, 5, 5, AMBER);
            rect(sp[k][0] - 1, sp[k][1] - 1, 3, 3, BLACK);
        }
    }
}

// Axis marker, bottom-left of the viewport: lit axes amber.
static void tripod(int x0, int y0, uint8_t bits) {
    const int ox = x0 + 15, oy = y0 + 50;
    line(ox, oy, ox + 8, oy + 4, (bits & 1) ? AMBER : DARK);
    line(ox, oy, ox - 8, oy + 4, (bits & 2) ? AMBER : DARK);
    line(ox, oy, ox, oy - 8, (bits & 4) ? AMBER : DARK);
    rect(ox - 1, oy - 1, 3, 3, WHITE);
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void param_visual(const ParamView &v, int x0, int y0) {
    const float val = v.field ? v.drawn : v.value;
    int bits = v.axis_bits;
    switch (v.visual) {
        case APP_PV_FILLET: {
            int r = (int)clampf(lroundf(fabsf(val) * 4), 0, 12);
            app_el_t e = r ? iso_el(32, 30, 16, 16, 12, 0, (uint8_t)(r | (val < 0 ? APP_ISO_ARG_CHAMFER : 0))) : iso_el(32, 30, 16, 16, 12, APP_ISO_EDGE);
            iso_box(e, x0, y0);
            break;
        }
        case APP_PV_CHAMFER: {
            int r = (int)clampf(lroundf(fabsf(val) * 2), 0, 12);
            app_el_t e = r ? iso_el(32, 30, 16, 16, 12, 0, (uint8_t)(r | APP_ISO_ARG_CHAMFER)) : iso_el(32, 30, 16, 16, 12, APP_ISO_EDGE);
            iso_box(e, x0, y0);
            break;
        }
        case APP_PV_SLIDE: {
            int d = (int)clampf(lroundf(val), -14, 14);
            iso_box(iso_el(26, 26, 12, 12, 10, APP_ISO_GHOST), x0, y0);
            iso_box(iso_el(26 + d, 26 + d / 2, 12, 12, 10, APP_ISO_SOLID), x0, y0);
            break;
        }
        case APP_PV_EXTRUDE: {
            int h = (int)lroundf(fabsf(val) * 2);
            if (val >= 0) {
                iso_box(iso_el(32, 40, 16, 16, (int)clampf(h, 0, 22), APP_ISO_FACE), x0, y0);
            } else {
                iso_box(iso_el(32, 36, 16, 16, 12, APP_ISO_GHOST), x0, y0);
                iso_box(iso_el(32, 36, 16, 16, (int)clampf(12 - h, 0, 12), APP_ISO_FACE), x0, y0);
            }
            break;
        }
        case APP_PV_OFFSET:
            if (val != 0) iso_box(iso_el(32, 30, 16, 16, 12, APP_ISO_GHOST), x0, y0);
            iso_box(iso_el(32, 30, 16, 16, (int)clampf(12 + lroundf(val * 2), 2, 24), APP_ISO_FACE), x0, y0);
            break;
        case APP_PV_HOLLOW:
            iso_box(iso_el(32, 30, 16, 16, 12, APP_ISO_HOLLOW, (uint8_t)clampf(1 + lroundf(val * 1.2f), 1, 7)), x0, y0);
            break;
        case APP_PV_MOVE: {
            Box3 g, m;
            g.ghost = true;
            float d = clampf(val * 1.5f, -16, 16);
            int n = (bits & 1) + ((bits >> 1) & 1) + ((bits >> 2) & 1);
            for (int i = 0; i < 3; i++) {
                if (bits & (1 << i)) m.move[i] = d * (i == 2 ? 0.8f : 1.0f) / sqrtf((float)(n ? n : 1));
            }
            box3(x0 + 32, y0 + 26, 12, 12, 10, g);
            box3(x0 + 32, y0 + 26, 12, 12, 10, m);
            tripod(x0, y0, bits);
            break;
        }
        case APP_PV_ROTATE: {
            Box3 r;
            r.axis = v.axis;
            r.angle = val * (float)M_PI / 180.0f;
            box3(x0 + 32, y0 + 24, 20, 12, 10, r);
            tripod(x0, y0, bits);
            break;
        }
        case APP_PV_SCALE: {
            Box3 g, s;
            g.ghost = true;
            s.grips = true;
            float f = clampf(val, 0.2f, 2.2f);
            for (int i = 0; i < 3; i++) if (bits & (1 << i)) s.scale[i] = f;
            if (val != 1) box3(x0 + 32, y0 + 30, 12, 12, 10, g);
            box3(x0 + 32, y0 + 30, 12, 12, 10, s);
            tripod(x0, y0, bits);
            break;
        }
        default: break;
    }
}

// "1.25", ".85", "-.85", "35".
static void format_value(char *buf, size_t n, float v, int decimals) {
    char tmp[32];
    if (decimals < 0) decimals = 0;
    if (decimals > 3) decimals = 3;
    snprintf(tmp, sizeof(tmp), "%.*f", decimals, (double)clampf(fabsf(v), 0, 99999));
    const char *s = (decimals > 0 && tmp[0] == '0' && tmp[1] == '.') ? tmp + 1 : tmp;
    snprintf(buf, n, "%s%s", v < 0 && strcmp(s, decimals ? ".00" : "0") ? "-" : "", s);
}

void draw_param(const ParamView &v) {
    text(v.name, CX, 24, GREY, 1, CENTER);
    rect(60, 40, 120, 1, DARK);
    const int x0 = CX - CARD_W / 2 + v.nudge, y0 = 46;
    round_frame(x0, y0, CARD_W, CARD_H, DARK);
    rect(x0 + 63, y0 + 6, 1, 52, DARK);
    clip(x0 + 4, y0 + 4, 57, 56);
    param_visual(v, x0, y0);
    unclip();
    if (v.field) { // Onshape: the feature list, the new feature amber above the rollback bar
        static const uint8_t G[3] = {APP_GLYPH_SKETCH, APP_GLYPH_SOLID, APP_GLYPH_SOLID};
        static const uint8_t LEN[3] = {14, 18, 16};
        for (int i = 0; i < 3; i++) {
            app_el_t r = EL_FROW(0, 0, 0, 0, 0);
            r.y = (int8_t)i; r.arg = G[i]; r.w = LEN[i]; r.color = i == 2 ? APP_C_AMBER : APP_C_GREY;
            row(r, x0, y0);
        }
        app_el_t rb = EL_ROLLBACK(3);
        element(rb, x0, y0);
    } else if (v.axes) {
        modes(v.modes, x0, y0);
        static const char *const XYZ[3] = {"X", "Y", "Z"};
        for (int i = 0; i < 3; i++) {
            bool on = v.axis_bits & (1 << i);
            int x = x0 + 68 + i * 16;
            cut(x, y0 + 20, 14, 11, on ? AMBER : DARK);
            text(XYZ[i], x + 7, y0 + 22, on ? BLACK : GREY, 1, CENTER);
        }
    } else {
        modes(v.modes, x0, y0);
        app_el_t row_el = {};
        row_el.op = APP_EL_ROW; row_el.color = APP_C_WHITE; row_el.w = 18; row_el.h = APP_ROW_SEL;
        row_el.arg = APP_GLYPH_SOLID; row_el.d = 14;
        row(row_el, x0, y0);
    }

    text(v.label, CX, 114, AMBER, 1, CENTER);
    char val[40];
    format_value(val, sizeof(val), v.value, v.decimals);
    if (v.field && !v.typed && v.value > 0) { // A shows the change: +0.30
        char plus[44];
        snprintf(plus, sizeof(plus), "+%s", val);
        strcpy(val, plus);
    }
    int w = text(val, CX, 126, WHITE, 3, CENTER);
    if (v.degrees) frame_box((int)lroundf(CX + w / 2.0f) + 3, 126, 5, 5, WHITE);

    // FREE / F1 .05 / F2 .10 / F4 1.0 -- the held one amber. Number field: F1 / KNOB / F4.
    char labels[4][48];
    int n = 0;
    if (!v.field) snprintf(labels[n++], sizeof(labels[0]), "FREE");
    static const char *const KEYS[3] = {"F1", "F2", "F4"};
    for (int i = 0; i < 3; i++) {
        char st[40];
        if (v.degrees) snprintf(st, sizeof(st), "%d", (int)lroundf(v.steps[i]));
        else if (v.steps[i] < 1) format_value(st, sizeof(st), v.steps[i], 2);
        else snprintf(st, sizeof(st), "%.1f", (double)v.steps[i]);
        snprintf(labels[n++], sizeof(labels[0]), "%s %s", v.field && i == 1 ? "KNOB" : KEYS[i], st);
    }
    const int lit = v.field ? v.step : v.step + 1;
    const int GAP = 10;
    int tw = -GAP;
    for (int i = 0; i < n; i++) tw += text_width(labels[i]) + GAP;
    float tx = lroundf(CX - tw / 2.0f);
    for (int i = 0; i < n; i++) {
        text(labels[i], tx, 157, i == lit ? AMBER : DARK);
        tx += text_width(labels[i]) + GAP;
    }
    if (v.f3 >= 0) {
        text(v.f3 >= 1 ? "RELEASE: CANCEL" : "RELEASE: OK", CX, 175, AMBER, 1, CENTER);
        rect(80, 189, 80, 2, DARK);
        rect(80, 189, (int)lroundf(80 * clampf(v.f3, 0, 1)), 2, AMBER);
    } else {
        text("F3 OK  HOLD F3 CANCEL", CX, 175, GREY, 1, CENTER);
        if (v.axes) text("TAP F1 F2 F4: X Y Z", CX, 189, GREY, 1, CENTER);
        if (v.field) text(v.typed ? "F2  B: TYPE" : "F2  A: SCROLL", CX, 189, GREY, 1, CENTER);
    }
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

// A command that types text: a little terminal typing it, a character per 70 ms, the cursor
// blinking once it's done. The Enter that sends it is F1's (the macros never press it).
static void typed_card(const char *s, int x, int y, uint32_t t_ms) {
    round_frame(x, y, CARD_W, CARD_H, DARK);
    disc(x + 8, y + 7, 1.5f, 0xFF5F57);
    disc(x + 14, y + 7, 1.5f, 0xFEBC2E);
    disc(x + 20, y + 7, 1.5f, 0x28C840);
    rect(x + 3, y + 13, CARD_W - 6, 1, DARK);
    int len = (int)strlen(s);
    uint32_t cycle = (uint32_t)len * 70 + 1800, m = t_ms % cycle;
    int shown = (int)(m / 70) < len ? (int)(m / 70) : len;
    char buf[48];
    snprintf(buf, sizeof(buf), "%.*s", shown < 47 ? shown : 47, s);
    text(">", x + 8, y + 24, AMBER);
    clip(x + 3, y + 14, CARD_W - 6, CARD_H - 17);
    int w = shown ? text(buf, x + 18, y + 24, WHITE) : 0;
    if (shown < len || (m / 420) % 2 == 0) rect(x + 19 + w, y + 23, 4, 9, AMBER);
    unclip();
    if (shown == len) text("F1 SENDS", x + CARD_W - 8, y + 47, GREY, 1, RIGHT);
}

// A shortcut: the chord on big keycaps, pressed now and then -- the modifiers go down, the key
// is struck (`repeat` times, one beat each, for a sequence like Esc Esc), everything comes up.
// Mac order: Control, Option, Shift, Command, the key.
static void chord_card(uint8_t modifier, const char *key, int repeat, int x, int y, uint32_t t_ms) {
    round_frame(x, y, CARD_W, CARD_H, DARK);
    const Sprite *mods[4];
    int n = 0;
    if (modifier & (0x01 | 0x10)) mods[n++] = &G_CTRL;
    if (modifier & (0x04 | 0x40)) mods[n++] = &G_OPT;
    if (modifier & (0x02 | 0x20)) mods[n++] = &G_SHIFT;
    if (modifier & (0x08 | 0x80)) mods[n++] = &G_CMD;
    const int cap = 24, gap = 5, side = 3;
    const int keys = key ? (repeat < 1 ? 1 : repeat) : 0;
    int ks = 2, kw = 0, total = 0;
    for (; ks >= 1; ks--) { // the key's label at 2x if the whole chord still fits, else 1x
        kw = key ? text_width(key, ks) + 14 : 0;
        if (key && kw < cap) kw = cap;
        total = n * (cap + gap) + keys * (kw + gap) - gap;
        if (total <= CARD_W - 12) break;
    }
    if (ks < 1) ks = 1;
    const uint32_t m = t_ms % 1600, start = 900, beat = 220;
    const bool mods_down = m >= start && m < start + beat * keys + 60;
    int kx = x + (CARD_W - total) / 2;
    const int ky0 = y + (CARD_H - cap - side) / 2;
    for (int i = 0; i < n + keys; i++) {
        bool is_key = i >= n;
        bool down = is_key ? (m >= start + beat * (uint32_t)(i - n) && m < start + beat * (uint32_t)(i - n) + 150) : mods_down;
        int w = is_key ? kw : cap, ky = ky0 + (down ? side : 0);
        if (!down) cut(kx, ky + side, w, cap, 0x1C1C1C); // the keycap's side
        cut(kx, ky, w, cap, down ? AMBER : DARK);
        uint32_t ink = down ? BLACK : WHITE;
        if (is_key) text(key, kx + w / 2.0f, ky + (cap - cap_height(ks)) / 2, ink, ks, CENTER);
        else sprite(*mods[i], kx + (cap - mods[i]->w * 2) / 2, ky + (cap - mods[i]->h * 2) / 2, ink, 2);
        kx += w + gap;
    }
}

static void entry_card(const app_scene_t *scene, const char *typed, bool chord, uint8_t mod, const char *key,
                       int repeat, int x, int y, uint32_t t_ms) {
    if (typed) typed_card(typed, x, y, t_ms);
    else if (chord) chord_card(mod, key, repeat, x, y, t_ms);
    else draw_card(scene, x, y, t_ms);
}

void draw_wheel(const WheelView &v) {
    text(v.ring_name, CX, 24, GREY, 1, CENTER);
    rect(60, 40, 120, 1, DARK);

    // The card slides like the PROFILE carousel: the old one leaves, the new one comes in.
    const int CARD_Y = 50, TRAVEL = 128;
    int off = (int)lroundf((1 - v.slide) * v.slide_dir * TRAVEL);
    if (v.prev_valid && v.slide < 1)
        entry_card(v.prev, v.prev_typed, v.prev_chord, v.prev_modifier, v.prev_key, v.prev_chord_repeat,
                   CX - CARD_W / 2 + off - v.slide_dir * TRAVEL, CARD_Y, 100000);
    entry_card(v.scene, v.typed, v.chord, v.modifier, v.key, v.chord_repeat, CX - CARD_W / 2 + off, CARD_Y, v.t_ms);
    if (v.entry > 0) sprite(SPR_TRI_L_M, 22, CARD_Y + 28, AMBER);
    if (v.entry < v.count - 1) sprite(SPR_TRI_R_M, 214, CARD_Y + 28, AMBER);

    text(v.name, CX, 122, WHITE, fit_scale(v.name, 170, 2), CENTER);
    if (v.entry == 0) text("RELEASE TO CLOSE", CX, 148, GREY, 1, CENTER);
    else if (v.typed) text("RELEASE TO TYPE", CX, 148, GREY, 1, CENTER);
    else if (v.chord) text("RELEASE TO PRESS", CX, 148, GREY, 1, CENTER);
    else if (v.hint) text(v.hint, CX, 148, AMBER, 1, CENTER);
    else if (v.search) draw_chord(v.modifier, v.key, CX, 145, "SEARCH");
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
