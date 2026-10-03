#include "ui_vinyl.hpp"
#include "ui_gfx.hpp"
#include <math.h>
#include <string.h>
extern "C" {
#include "user_prefs.h"
}

namespace ui {

namespace {

constexpr int FW = 240;       // the frame's width
constexpr int TURN = 1024;    // angle units in a turn, clockwise from 3 o'clock
constexpr int LIGHT = 967;    // the reflection's axis, -0.35 rad: a bow tie from 2 to 8 o'clock,
                              // clear of the title
constexpr int WOBBLE = 8;     // how far a slightly warped record sways it (0.05 rad)
constexpr int BASE_ROWS = 236; // the base: up to 236 rows of 236 pixels, two to a byte
constexpr int BASE_STRIDE = 118;
constexpr int PQ = 118;       // the polar table's side (one quadrant)
constexpr int RUNS = 4;       // per base row, where the light can reach: two lobes, each cut by the label

// A layout's record: radius, label radius, groove pitch (px).
struct Geo {
    int R, Rl, pitch;
};
Geo geo(int style) {
    switch (style) {
        case COVER_RECORD: return {118, 54, 3};
        case COVER_SLIDE: return {52, 21, 2};
        default: return {74, 30, 3};
    }
}
int sleeve_size(int style) { return style == COVER_SLIDE ? 104 : style == COVER_BLEED ? 150 : 0; }
int wear_radius(int style) { return style == COVER_SLIDE ? 49 : 71; } // ring wear on the sleeve

// What a ring of the record is made of, in sixteenths of a ramp step: its tone in the dark,
// how much the light adds, and whether the grain shows in it.
struct Ring {
    uint8_t base, gain;
    bool grain;
};

// 10 greys, black to white.
const uint32_t GREYS[10] = {0x000000, 0x0E0E0E, 0x1A1A1A, 0x262626, 0x3A3A3A, 0x545454, 0x6E6E6E, 0x9A9A9A, 0xC8C8C8, 0xFFFFFF};

// Read for every pixel: in internal RAM (~5.5 KB), not PSRAM. Not const, so they stay out of flash.
uint8_t s_bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5}; // 4x4 ordered dither
uint16_t s_ramp[16];          // GREYS as RGB565 big-endian (16: a base nibble can't index past it)
Ring s_ring[118];             // by ring (whole pixels from the centre)
uint8_t s_ub[256];            // by distance in half pixels: how far out (0..15), for the light's width
uint8_t s_light[16][64];      // (1 - da / hw)^1.5 * 255, by that and distance from the axis
uint8_t s_grain[60][64];      // groove grain: per two rings, 64 cells round
uint8_t s_label_hw[108];      // per label row: half its width, as the record's polar table draws it

// Read once a row: in PSRAM with the rest (VinylMem::tables).
struct Tables {
    int16_t span[BASE_ROWS][2];         // each base row: the record, [x0, x1)
    int16_t wedge[BASE_ROWS][RUNS * 2]; // each base row: runs the light can reach, [x0, x1)
};
static_assert(sizeof(Tables) <= VINYL_TABLES_BYTES, "VINYL_TABLES_BYTES is too small");

VinylMem M;
Tables *T = nullptr;
bool s_ready = false;  // the polar table and the light / grain tables are made
int s_base_style = -1; // the layout `base` holds

inline int tone_index(int v16, int x, int y) {
    if (v16 < 0) v16 = 0;
    if (v16 > 144) v16 = 144;
    int i = (v16 >> 4) + ((v16 & 15) > s_bayer[(y & 3) * 4 + (x & 3)]);
    return i > 9 ? 9 : i;
}

// Distance (in half pixels) and angle (TURN units) of pixel (dx, dy) from the centre, which sits
// between pixels: dx = 0 is the first pixel right of it.
inline void polar(int dx, int dy, int &d2, int &a) {
    int qx = dx >= 0 ? dx : -dx - 1, qy = dy >= 0 ? dy : -dy - 1;
    uint16_t e = M.polar[qy * PQ + qx];
    int aq = e & 255;
    d2 = e >> 8;
    a = dy >= 0 ? (dx >= 0 ? aq : 511 - aq) : (dx >= 0 ? 1023 - aq : 512 + aq);
}

inline int light(int a, int ub, int axis) {
    int m = (a - axis) & 511, da = m < 256 ? m : 512 - m; // the bow tie: both sides of the axis
    return da < 64 ? s_light[ub][da] : 0;
}

inline int grain(int ring2, int rel) {
    int cell = rel >> 4, f = rel & 15;
    return (s_grain[ring2][cell] * (16 - f) + s_grain[ring2][(cell + 1) & 63] * f) >> 4;
}

// g: the grain (0..255), or -1 for none (the base, drawn once).
inline int value(const Ring &r, int L, int g) {
    int gf = r.grain && g >= 0 ? 102 + (g * 307 >> 8) : 256;
    return r.base + (L * r.gain * gf >> 16);
}

inline void put(uint16_t *fb, int x, int y, uint16_t v) {
    if (fb) fb[y * FW + x] = v;
    else put565(x, y, v);
}

void make_tables() {
    for (int qy = 0; qy < PQ; qy++)
        for (int qx = 0; qx < PQ; qx++) {
            float d = hypotf(qx + 0.5f, qy + 0.5f);
            int d2 = (int)(2 * d), aq = (int)(atan2f(qy + 0.5f, qx + 0.5f) / (float)M_PI_2 * 256);
            M.polar[qy * PQ + qx] = (uint16_t)((d2 > 255 ? 255 : d2) << 8 | (aq > 255 ? 255 : aq));
        }
    for (int ub = 0; ub < 16; ub++) {
        float u = (ub + 0.5f) / 16, hw = (0.10f + 0.26f * u) * TURN / (2 * (float)M_PI);
        for (int da = 0; da < 64; da++) s_light[ub][da] = da < hw ? (uint8_t)(powf(1 - da / hw, 1.5f) * 255) : 0;
    }
    for (int r = 0; r < 60; r++)
        for (int c = 0; c < 64; c++) s_grain[r][c] = (uint8_t)(rnd(r * 64 + c, 41) * 255);
    for (int i = 0; i < 16; i++) {
        uint32_t c = GREYS[i < 10 ? i : 9];
        uint16_t v = (uint16_t)((c >> 19 & 31) << 11 | (c >> 10 & 63) << 5 | (c >> 3 & 31));
        s_ramp[i] = (uint16_t)(v << 8 | v >> 8);
    }
    s_ready = true;
}

void make_rings(const Geo &g) {
    const float span = g.R - g.Rl - 8;
    static const float GAPS[3] = {0.27f, 0.53f, 0.78f}; // the quiet grooves between tracks
    for (int ri = 0; ri < g.R; ri++) {
        float d = ri + 0.5f;
        Ring &r = s_ring[ri];
        if (g.R - d < 1) r = {56, 64, false};                                        // the rim
        else if (g.R - d < 3) r = {10, 64, false};                                   // lead-in
        else if (d < g.Rl + 5) r = {(uint8_t)(ri == g.Rl + 3 ? 48 : 10), 48, false}; // run-out groove
        else {
            float w = (d - g.Rl - 5) / span;
            bool gap = false;
            for (float gp : GAPS) gap |= fabsf(w - gp) * span < 1.2f;
            if (gap) r = {5, 104, false};
            else if (ri % g.pitch == 0) r = {45, 104, true};
            else r = {21, 56, true};
        }
    }
    for (int d2 = 0; d2 < 256; d2++) {
        int ub = (d2 - 2 * g.Rl) * 16 / (2 * (g.R - g.Rl));
        s_ub[d2] = (uint8_t)(ub < 0 ? 0 : ub > 15 ? 15 : ub);
    }
    // The label's rows, as wide as the polar table makes its edge (so no seam with the grooves).
    for (int ly = 0; ly < 2 * g.Rl; ly++) {
        int hw = 0;
        for (int lx = g.Rl; lx < 2 * g.Rl; lx++) {
            int d2, a;
            polar(lx - g.Rl, ly - g.Rl, d2, a);
            if (d2 >= 2 * g.Rl) break;
            hw = lx - g.Rl + 1;
        }
        s_label_hw[ly] = (uint8_t)hw;
    }
}

// The record without its label, lit but without grain (ramp indices), and where the light can
// reach.
void make_base(const Geo &g) {
    make_rings(g);
    const int R = g.R;
    memset(M.base, 0, VINYL_BASE_BYTES);
    for (int by = 0; by < 2 * R; by++) {
        int x0 = -1, x1 = -1, nw = 0, start = -1;
        int16_t *w = T->wedge[by];
        memset(w, 0, sizeof(T->wedge[by]));
        uint8_t *row = M.base + by * BASE_STRIDE;
        for (int bx = 0; bx <= 2 * R; bx++) {
            bool lit = false;
            if (bx < 2 * R) {
                int d2, a;
                polar(bx - R, by - R, d2, a);
                if (d2 < 2 * R) {
                    if (x0 < 0) x0 = bx;
                    x1 = bx + 1;
                    if (d2 >= 2 * g.Rl) {
                        const Ring &r = s_ring[d2 >> 1];
                        int ub = s_ub[d2];
                        row[bx >> 1] |= (uint8_t)(tone_index(value(r, light(a, ub, LIGHT), -1), bx, by) << ((bx & 1) * 4));
                        for (int k = -WOBBLE; k <= WOBBLE && !lit; k += WOBBLE) lit = light(a, ub, LIGHT + k) > 0;
                    }
                }
            }
            if (lit && start < 0) start = bx;
            if (!lit && start >= 0) {
                if (nw < RUNS) {
                    w[nw * 2] = (int16_t)start;
                    w[nw * 2 + 1] = (int16_t)bx;
                    nw++;
                }
                start = -1;
            }
        }
        T->span[by][0] = (int16_t)(x0 < 0 ? 0 : x0);
        T->span[by][1] = (int16_t)(x1 < 0 ? 0 : x1);
    }
}

// The cover averaged down to s x s (RGB565 BE in and out).
void average(const uint16_t *cover, int s, uint16_t *out) {
    const float f = 240.0f / s;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            int sx0 = (int)(x * f), sx1 = (int)((x + 1) * f), sy0 = (int)(y * f), sy1 = (int)((y + 1) * f);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int yy = sy0; yy < sy1; yy++)
                for (int xx = sx0; xx < sx1; xx++) {
                    uint16_t v = cover[yy * 240 + xx];
                    v = (uint16_t)(v << 8 | v >> 8);
                    r += (v >> 11) & 31;
                    g += (v >> 5) & 63;
                    b += v & 31;
                    n++;
                }
            uint16_t v = (uint16_t)((r + n / 2) / n << 11 | (g + n / 2) / n << 5 | (b + n / 2) / n);
            out[y * s + x] = (uint16_t)(v << 8 | v >> 8);
        }
}

// Ring wear: a faint dithered circle where a record has rubbed the sleeve.
void ring_wear(uint16_t *t, int s, int r) {
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            if ((x + y) & 1) continue;
            float d = hypotf(x + 0.5f - s / 2.0f, y + 0.5f - s / 2.0f);
            if (fabsf(d - r) >= 1) continue;
            uint16_t v = t[y * s + x];
            v = (uint16_t)(v << 8 | v >> 8);
            int R5 = (v >> 11) & 31, G6 = (v >> 5) & 63, B5 = v & 31;
            R5 += (31 - R5) * 22 / 100;
            G6 += (63 - G6) * 22 / 100;
            B5 += (31 - B5) * 22 / 100;
            v = (uint16_t)(R5 << 11 | G6 << 5 | B5);
            t[y * s + x] = (uint16_t)(v << 8 | v >> 8);
        }
}

void record(int cx, int cy, const Geo &g, float theta) {
    uint16_t *fb = frame565();
    const int R = g.R, ox = cx - R, oy = cy - R;
    // The grooves and the light, as drawn once: a nibble per pixel through the ramp.
    for (int by = 0; by < 2 * R; by++) {
        int y = oy + by;
        if (y < 0 || y >= FW) continue;
        int a = T->span[by][0], b = T->span[by][1];
        if (ox + a < 0) a = -ox;
        if (ox + b > FW) b = FW - ox;
        const uint8_t *row = M.base + by * BASE_STRIDE;
        if (fb) {
            uint16_t *dst = fb + y * FW + ox;
            for (int bx = a; bx < b; bx++) dst[bx] = s_ramp[(row[bx >> 1] >> ((bx & 1) * 4)) & 15];
        } else {
            for (int bx = a; bx < b; bx++) put565(ox + bx, y, s_ramp[(row[bx >> 1] >> ((bx & 1) * 4)) & 15]);
        }
    }
    // The light's wedge again, with the grain turning through it.
    float t = fmodf(theta, 2 * (float)M_PI);
    if (t < 0) t += 2 * (float)M_PI;
    const int th = (int)(t * TURN / (2 * (float)M_PI)) & (TURN - 1);
    const int axis = LIGHT + (int)lroundf(WOBBLE * sinf(t));
    for (int by = 0; by < 2 * R; by++) {
        int y = oy + by;
        if (y < 0 || y >= FW) continue;
        const int16_t *w = T->wedge[by];
        for (int k = 0; k < RUNS && w[k * 2 + 1] > w[k * 2]; k++) {
            int a0 = w[k * 2], a1 = w[k * 2 + 1];
            if (ox + a0 < 0) a0 = -ox;
            if (ox + a1 > FW) a1 = FW - ox;
            for (int bx = a0; bx < a1; bx++) {
                int d2, a;
                polar(bx - R, by - R, d2, a);
                if (d2 >= 2 * R || d2 < 2 * g.Rl) continue;
                const Ring &r = s_ring[d2 >> 1];
                int L = light(a, s_ub[d2], axis);
                int v = value(r, L, r.grain ? grain(d2 >> 2, (a - th) & (TURN - 1)) : -1);
                put(fb, ox + bx, y, s_ramp[tone_index(v, bx, by)]);
            }
        }
    }
    // Dust that turns with the record: dark specks, white where they cross the light.
    const int n = (int)lroundf(80 * (R / 118.0f) * (R / 118.0f));
    for (int i = 0; i < n; i++) {
        float d = g.Rl + 6 + rnd(i, 11) * (R - g.Rl - 9), a = rnd(i, 23) * 2 * (float)M_PI + theta;
        int x = (int)floorf(cx + cosf(a) * d), y = (int)floorf(cy + sinf(a) * d);
        if (x < 0 || y < 0 || x >= FW || y >= FW) continue;
        int d2, pa;
        polar(x - cx, y - cy, d2, pa);
        put(fb, x, y, light(pa, s_ub[d2], axis) > 51 ? 0xFFFF : s_ramp[tone_index(67, x - ox, y - oy)]);
    }
    // The label: the cover turned (nearest neighbour), stepping through it in 16.16 along a row.
    const int Rl = g.Rl, S = 2 * Rl;
    const float c = cosf(theta), s = sinf(theta);
    const int32_t dc = (int32_t)lroundf(c * 65536), ds = (int32_t)lroundf(s * 65536);
    for (int ly = 0; ly < S; ly++) {
        int y = cy - Rl + ly, hw = s_label_hw[ly];
        if (y < 0 || y >= FW || hw == 0) continue;
        int lx0 = Rl - hw, lx1 = Rl + hw;
        if (cx - Rl + lx0 < 0) lx0 = Rl - cx;
        if (cx - Rl + lx1 > FW) lx1 = FW - cx + Rl;
        if (lx1 <= lx0) continue;
        float qx = lx0 - Rl + 0.5f, qy = ly - Rl + 0.5f;
        int32_t u = (int32_t)lroundf((c * qx + s * qy + Rl) * 65536), v = (int32_t)lroundf((-s * qx + c * qy + Rl) * 65536);
        uint16_t *dst = fb ? fb + y * FW + (cx - Rl) : nullptr;
        for (int lx = lx0; lx < lx1; lx++, u += dc, v -= ds) {
            int su = u >> 16, sv = v >> 16;
            su = su < 0 ? 0 : su >= S ? S - 1 : su;
            sv = sv < 0 ? 0 : sv >= S ? S - 1 : sv;
            uint16_t px = M.label[sv * S + su];
            if (dst) dst[lx] = px;
            else put565(cx - Rl + lx, y, px);
        }
    }
    disc(cx, cy, 4, DARK); // the spindle
    disc(cx, cy, 2.5f, BLACK);
}

void sleeve(int sx, int sy, int S) {
    uint16_t *fb = frame565();
    for (int y = 0; y < S; y++) {
        int Y = sy + y;
        if (Y < 0 || Y >= FW) continue;
        int x0 = sx < 0 ? -sx : 0, x1 = sx + S > FW ? FW - sx : S;
        if (x1 <= x0) continue;
        const uint16_t *src = M.sleeve + y * S;
        if (fb) memcpy(fb + Y * FW + sx + x0, src + x0, (size_t)(x1 - x0) * 2);
        else
            for (int x = x0; x < x1; x++) put565(sx + x, Y, src[x]);
        if (y == 0 || y == S - 1) { // cut corners
            if (x0 == 0) rect(sx, Y, 1, 1, BLACK);
            if (x1 == S) rect(sx + S - 1, Y, 1, 1, BLACK);
        }
    }
}

// Where the sleeve's edge covers the record: two black columns, then one at half.
void edge_shadow(int x, int y, int h) {
    rect(x, y, 2, h, BLACK);
    uint16_t *fb = frame565();
    if (!fb || x + 2 < 0 || x + 2 >= FW) return;
    for (int yy = y < 0 ? 0 : y; yy < y + h && yy < FW; yy++) {
        uint16_t v = fb[yy * FW + x + 2];
        v = (uint16_t)(v << 8 | v >> 8);
        v = (uint16_t)(v >> 1 & 0x7BEF);
        fb[yy * FW + x + 2] = (uint16_t)(v << 8 | v >> 8);
    }
}

} // namespace

void vinyl_bind(const VinylMem &m) {
    M = m;
    T = (Tables *)m.tables;
    s_ready = false;
    s_base_style = -1;
}

void vinyl_prepare(int style, const uint16_t *cover_be) {
    if (style <= COVER_FLAT || style >= COVER_STYLE_COUNT || !M.polar || !T) return;
    if (!s_ready) make_tables();
    const Geo g = geo(style);
    if (s_base_style != style) {
        make_base(g);
        s_base_style = style;
    }
    if (!cover_be) return;
    average(cover_be, 2 * g.Rl, M.label);
    if (int S = sleeve_size(style)) {
        average(cover_be, S, M.sleeve);
        ring_wear(M.sleeve, S, wear_radius(style));
    }
}

void vinyl_draw(int style, float theta, float out) {
    if (style <= COVER_FLAT || style >= COVER_STYLE_COUNT || s_base_style != style) return;
    const Geo g = geo(style);
    switch (style) {
        case COVER_RECORD: record(CX, CY, g, theta); break;
        case COVER_SLIDE: {
            const int S = 104, sx = (int)lroundf(CX - 52 - 26 * out), sy = 46;
            record(sx + 52 + (int)lroundf(52 * out), sy + 52, g, theta);
            edge_shadow(sx + S, sy, S);
            sleeve(sx, sy, S);
            break;
        }
        case COVER_BLEED: {
            const int S = 150, sx = (int)lroundf(45 - 60 * out), sy = 28;
            record(sx + 75 + (int)lroundf(74 * out), sy + 75, g, theta);
            edge_shadow(sx + S, sy, S);
            sleeve(sx, sy, S);
            break;
        }
    }
}

} // namespace ui
