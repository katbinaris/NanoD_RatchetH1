#include "ui_extras.hpp"
#include "ui_gfx.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include "ui_state.h"
}

namespace ui {

static const uint32_t ALLOW_GREEN = 0x22DD66u, DENY_RED = 0xFF3B30u, PANEL = 0x111111u;

static uint32_t scale_rgb(uint32_t c, float k) {
    if (k > 1) k = 1;
    uint32_t r = (uint32_t)(((c >> 16) & 0xFF) * k), g = (uint32_t)(((c >> 8) & 0xFF) * k), b = (uint32_t)((c & 0xFF) * k);
    return r << 16 | g << 8 | b;
}

static float breath(uint32_t t_ms, float period_ms) {
    return 0.5f - 0.5f * cosf(t_ms * 2 * (float)M_PI / period_ms);
}

// --- LIGHTS ---

void draw_lights(const menu_render_snapshot_t &snap, const LightsInputs &in) {
    // The rim mirrors the LED ring. The LEDs run at a few % of full brightness, so the frame
    // is scaled up as a whole (relative levels kept: a comet still looks like a comet).
    if (in.ring) {
        int peak = 0;
        for (int i = 0; i < 60; i++)
            for (int k = 0; k < 3; k++) peak = in.ring[i][k] > peak ? in.ring[i][k] : peak;
        float gain = peak ? 255.0f / peak : 0;
        if (gain > 12) gain = 12;
        for (int i = 0; i < 60; i++) {
            float a = i * 2 * (float)M_PI / 60 - (float)M_PI / 2;
            float x = CX + cosf(a) * 112, y = CY + sinf(a) * 112;
            uint32_t c = (uint32_t)fminf(in.ring[i][0] * gain, 255) << 16 | (uint32_t)fminf(in.ring[i][1] * gain, 255) << 8
                       | (uint32_t)fminf(in.ring[i][2] * gain, 255);
            if (c == 0) frame_box((int)lroundf(x) - 2, (int)lroundf(y) - 2, 5, 5, DARK);
            else cut((int)lroundf(x) - 2, (int)lroundf(y) - 2, 5, 5, c);
        }
    }

    text("LIGHTS", CX, 26, GREY, 1, CENTER);
    rect(70, 40, 100, 1, DARK);
    for (int i = 0; i < snap.row_count && i < MENU_LIGHTS_ROW_COUNT; i++) {
        const menu_render_row_t &r = snap.rows[i];
        const int y = 52 + i * 16;
        if (r.selected) cut(42, y - 4, 156, 15, AMBER);
        uint32_t lc = r.selected ? BLACK : GREY;
        uint32_t vc = r.selected ? BLACK : r.muted ? DARK : WHITE;
        int lw = text(r.label, 50, y, lc);
        text(r.value, 190, y, vc, 1, RIGHT);
        if (i == MENU_LIGHTS_ROW_COLOR) {
            float sx = 50 + lw + 10, sy = y + 3;
            disc(sx, sy, 4, in.swatch);
            if (r.selected) ring(sx, sy, 5, BLACK, 1);
        }
    }
    if (snap.dirty && in.blink_on) text("F2 SAVE", CX, 154, AMBER, 1, CENTER);
    else text("TURN  F1 NEXT  F3 BACK", CX, 154, DARK, 1, CENTER);
}

// --- agent notification ---

// The agents' marks, drawn black on the coloured badge.
static const Sprite MARK[4] = {
    {9, 9, // Claude: the spark
     "....#...."
     ".#..#..#."
     "..#.#.#.."
     "...###..."
     "#########"
     "...###..."
     "..#.#.#.."
     ".#..#..#."
     "....#...."},
    {9, 9, // Codex: a prompt
     "........."
     "##......."
     ".##......"
     "..##....."
     "...##...."
     "..##....."
     ".##......"
     "##..#####"
     "........."},
    {8, 10, // Cursor: the pointer
     "#......."
     "##......"
     "###....."
     "####...."
     "#####..."
     "######.."
     "#######."
     "####...."
     "#..##..."
     "....##.."},
    {9, 9, // anything else: a bell
     "....#...."
     "...###..."
     "..#####.."
     "..#####.."
     "..#####.."
     ".#######."
     "#########"
     "........."
     "...###..."},
};

// Rings of light just inside the glass: solid, then two dithered rings fading inward -- a glow
// in whole pixels. Several agents waiting: one arc each in queue order from 12 o'clock, the
// first breathing, the rest steady.
static void glow(const uint32_t *cols, int n, float b) {
    static const float R[3] = {118, 114, 110}, LEVEL[3] = {1.0f, 0.55f, 0.3f};
    static const int STEP[3] = {1, 2, 3};
    for (int k = 0; k < 3; k++) {
        int steps = (int)(2 * (float)M_PI * R[k]);
        for (int s = 0; s < steps; s += STEP[k]) {
            float u = (float)s / steps; // 0..1 clockwise from 12 o'clock
            int seg = n > 1 ? (int)(u * n) : 0;
            if (n > 1 && fmodf(u * n, 1.0f) < 0.012f * n) continue; // a gap between arcs
            float lv = LEVEL[k] * (seg == 0 ? 0.25f + 0.75f * b : 0.45f);
            float a = u * 2 * (float)M_PI - (float)M_PI / 2;
            int th = k == 0 ? 2 : 1;
            rect(CX + cosf(a) * R[k] - th / 2.0f, CY + sinf(a) * R[k] - th / 2.0f, th, th, scale_rgb(cols[seg], lv));
        }
    }
}

// Holding F1: a thick green arc from 12 o'clock with a bright head.
static void hold_arc(float frac) {
    const float r = 115;
    int n = (int)(frac * 2 * (float)M_PI * r);
    float a = 0;
    for (int s = 0; s <= n; s++) {
        a = s / r - (float)M_PI / 2;
        rect(CX + cosf(a) * r - 2.5f, CY + sinf(a) * r - 2.5f, 5, 5, ALLOW_GREEN);
    }
    disc(CX + cosf(a) * r, CY + sinf(a) * r, 4, WHITE);
}

// Greedy word wrap to `max_w`, at most `lines` lines; a word too long for a line is cut, and
// text that doesn't fit ends in "..".
static int wrap(const char *s, int max_w, char out[][48], int lines) {
    int n = 0;
    while (*s && n < lines) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len = 0, fit = 0;
        while (s[len] && len < 47) {
            char buf[48];
            memcpy(buf, s, len + 1);
            buf[len + 1] = '\0';
            if (text_width(buf) > max_w) break;
            len++;
            if (s[len] == ' ' || s[len] == '\0') fit = len;
        }
        if (fit == 0) fit = len > 0 ? len : 1;
        memcpy(out[n], s, fit);
        out[n][fit] = '\0';
        s += fit;
        n++;
    }
    while (*s == ' ') s++;
    if (*s && n == lines) {
        size_t l = strlen(out[n - 1]);
        if (l > 2) memcpy(out[n - 1] + l - 2, "..", 3);
    }
    return n;
}

void draw_notify(const NotifyInputs &in) {
    const float b = breath(in.t_ms, 2400);
    uint32_t one[1] = {in.color};
    glow(in.waiting > 1 && in.queue ? in.queue : one, in.waiting > 1 && in.queue ? in.waiting : 1, b);
    if (in.hold > 0) hold_arc(in.hold);

    // Badge: the agent's mark and name, black on its colour.
    const Sprite &mark = MARK[in.agent >= 0 && in.agent < 4 ? in.agent : 3];
    int tw = text_width(in.source), bw = 7 + mark.w + 5 + tw + 8;
    int bx = (int)lroundf(CX - bw / 2.0f);
    cut(bx, 19, bw, 16, in.color);
    sprite(mark, bx + 7, 19 + (16 - mark.h) / 2, BLACK);
    text(in.source, bx + 7 + mark.w + 5, 24, BLACK);

    text(in.title, CX, 44, WHITE, fit_scale(in.title, 176, 3), CENTER);

    // The command (or message) in a terminal-like panel.
    char lines[3][48];
    cut(28, 72, 184, 44, PANEL);
    frame_box(28, 72, 184, 44, DARK);
    if (in.ask) {
        int n = wrap(in.body, 150, lines, 3);
        text(">", 36, 77, in.color);
        for (int i = 0; i < n; i++) text(lines[i], 46, 77 + i * 12, WHITE);
    } else {
        int n = wrap(in.body, 168, lines, 3);
        int y0 = 94 - n * 6;
        for (int i = 0; i < n; i++) text(lines[i], CX, y0 + i * 12, GREY, 1, CENTER);
    }

    // Who else is waiting: one dot each, in queue order.
    if (in.waiting > 1 && in.queue) {
        int n = in.waiting > 8 ? 8 : in.waiting;
        float x0 = CX - (n - 1) * 5.0f;
        for (int i = 0; i < n; i++) disc(x0 + i * 10, 124, i == 0 ? 3 : 2, in.queue[i]);
    }

    static const char *const keys[4] = {"F1", "F2", "F3", "F4"};
    static const int xs[4] = {60, 100, 140, 180};
    static const int ys[4] = {146, 154, 154, 146};
    if (in.ask) {
        bool holding = in.hold > 0;
        text(holding ? "KEEP HOLDING" : "HOLD F1 TO ALLOW", CX, 132, holding ? ALLOW_GREEN : GREY, 1, CENTER);
        static const char *const acts[4] = {"ALLOW", "LATER", "DENY", "LATER"};
        for (int i = 0; i < 4; i++) {
            keycap(xs[i] - KEY_W / 2, ys[i], keys[i], (in.buttons >> i) & 1, false);
            uint32_t c = i == 0 ? ALLOW_GREEN : i == 2 ? DENY_RED : WHITE;
            text(acts[i], xs[i], ys[i] + KEY_H + 5, c, 1, CENTER);
        }
    } else {
        for (int i = 0; i < 4; i++) keycap(xs[i] - KEY_W / 2, ys[i], keys[i], (in.buttons >> i) & 1, false);
        text("ANY KEY TO DISMISS", CX, 182, GREY, 1, CENTER);
    }
}

// --- MUSIC: now playing ---

// A triangle h px tall pointing right (or left), h/2+1 wide, top-left at (x, y).
static void tri(float x, float y, int h, bool right, uint32_t c) {
    int half = h / 2;
    for (int i = 0; i < h; i++) {
        int w = (i <= half ? i : h - 1 - i) + 1;
        rect(right ? x : x + half + 1 - w, y + i, w, 1, c);
    }
}

static void glyph(int g, float cx, float cy, uint32_t c) {
    const int h = 27, w = h / 2 + 1;
    switch (g) {
        case NP_GLYPH_PLAY: tri(cx - w / 2.0f + 2, cy - h / 2.0f, h, true, c); break;
        case NP_GLYPH_PAUSE:
            rect(cx - 10, cy - 12, 7, 25, c);
            rect(cx + 3, cy - 12, 7, 25, c);
            break;
        case NP_GLYPH_NEXT:
            tri(cx - w - 2, cy - h / 2.0f, h, true, c);
            tri(cx - 2, cy - h / 2.0f, h, true, c);
            rect(cx + w - 2, cy - h / 2.0f, 4, h, c);
            break;
        case NP_GLYPH_PREV:
            rect(cx - w - 2, cy - h / 2.0f, 4, h, c);
            tri(cx - w + 2, cy - h / 2.0f, h, false, c);
            tri(cx + 2, cy - h / 2.0f, h, false, c);
            break;
        default: break;
    }
}

void draw_now_playing(const NowPlayingInputs &in) {
    if (!in.has_cover && in.icon48) image565(CX - 48, 46, 48, 48, in.icon48, 1.0f, 2); // stands in for the cover

    // Title and artist on the darkened lower part of the cover.
    const char *title = in.title && in.title[0] ? in.title : "NOW PLAYING";
    text(title, CX, 174, WHITE, fit_scale(title, 172, 2), CENTER);
    if (in.artist && in.artist[0]) text(in.artist, CX, 194, 0xBDBDBD, 1, CENTER);

    if (!in.playing) { // paused: a small badge at the top
        int w = text_width("PAUSED") + 22;
        cut((int)(CX - w / 2.0f), 22, w, 15, 0x000000);
        frame_box((int)(CX - w / 2.0f), 22, w, 15, DARK);
        rect(CX - w / 2.0f + 6, 26, 2, 7, WHITE);
        rect(CX - w / 2.0f + 10, 26, 2, 7, WHITE);
        text("PAUSED", CX - w / 2.0f + 16, 26, WHITE);
    }

    // Volume: a ring round the glass and the number in the middle, while the knob turns.
    if (in.volume_k > 0 && in.volume >= 0) {
        const float r = 113;
        int steps = (int)(2 * (float)M_PI * r), fill = (int)(steps * in.volume / 100.0f);
        for (int s = 0; s < steps; s += 2) {
            float a = (float)s / steps * 2 * (float)M_PI - (float)M_PI / 2;
            rect(CX + cosf(a) * r - 1, CY + sinf(a) * r - 1, 3, 3, scale_rgb(DARK, in.volume_k));
        }
        float a = 0;
        for (int s = 0; s <= fill; s++) {
            a = (float)s / steps * 2 * (float)M_PI - (float)M_PI / 2;
            rect(CX + cosf(a) * r - 2.5f, CY + sinf(a) * r - 2.5f, 5, 5, scale_rgb(in.accent, in.volume_k));
        }
        if (fill > 0) disc(CX + cosf(a) * r, CY + sinf(a) * r, 3.5f, scale_rgb(WHITE, in.volume_k));
        shade_disc(CX, CY - 8, 30, 0.85f * in.volume_k); // fades with the ring: no dark spot left behind
        char v[8];
        snprintf(v, sizeof(v), "%d", in.volume);
        text("VOL", CX, CY - 27, scale_rgb(GREY, in.volume_k), 1, CENTER);
        text(v, CX, CY - 15, scale_rgb(WHITE, in.volume_k), 3, CENTER);
    } else if (in.glyph != NP_GLYPH_NONE && in.glyph_k > 0) { // a media key, just pressed
        shade_disc(CX, CY - 8, 30, 0.85f * in.glyph_k);
        glyph(in.glyph, CX, CY - 8, scale_rgb(WHITE, in.glyph_k));
    }
}

// --- AGENTS: the dashboard ---

void draw_agent_board(const AgentRowView *rows, int n, uint32_t t_ms) {
    if (n == 0) {
        text("NO AGENTS RUNNING", CX, 86, GREY, 1, CENTER);
        text("CLAUDE  CODEX  CURSOR", CX, 102, DARK, 1, CENTER);
        return;
    }
    static const char *const STATE[4] = {"IDLE", "WORKING", "YOUR TURN", "ASKING"};
    const int y0 = n <= 2 ? 78 : 62;
    for (int i = 0; i < n && i < 4; i++) {
        const AgentRowView &r = rows[i];
        int y = y0 + i * 19;
        int st = r.state >= 0 && r.state < 4 ? r.state : 0;
        bool blink = ((t_ms / 450) % 2) == 0;
        disc(52, y + 3, 3, st == 3 && !blink ? scale_rgb(r.color, 0.35f) : r.color);
        text(r.name, 62, y, WHITE);
        uint32_t sc = st == 1 ? AMBER : st == 2 ? WHITE : st == 3 ? (blink ? r.color : WHITE) : GREY;
        if (st == 1) { // working: three dots that walk
            int w = text(STATE[st], 178, y, sc, 1, RIGHT);
            (void)w;
            for (int d = 0; d < 3; d++) rect(182 + d * 4, y + 5, 2, 2, (int)((t_ms / 300) % 4) > d ? AMBER : DARK);
        } else {
            text(STATE[st], 190, y, sc, 1, RIGHT);
        }
    }
}

} // namespace ui
