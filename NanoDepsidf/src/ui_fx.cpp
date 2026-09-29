#include "ui_fx.hpp"
#include "ui_gfx.hpp"
#include <math.h>
#include <string.h>

namespace ui {

static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline float lerpf(float a, float b, float k) { return a + (b - a) * k; }
static inline float ease_out(float k) { k = clampf(k, 0, 1); return 1 - (1 - k) * (1 - k) * (1 - k); }
static inline float ease_in_out(float k) {
    k = clampf(k, 0, 1);
    return k < 0.5f ? 4 * k * k * k : 1 - powf(-2 * k + 2, 3) / 2;
}
static inline bool in_circle(float x, float y, float r) {
    return (x - CX) * (x - CX) + (y - CY) * (y - CY) <= r * r;
}

// --- loading screen: Big bang ---

// "QUADRA" at scale 3 is ~130 blocks; room to spare.
#define LOGO_MAX_BLOCKS 256
static int16_t s_logo[LOGO_MAX_BLOCKS][2];
static int s_logo_n = 0;
// Per-block scatter point and return delay, fixed at init (same hash as the mockup).
static int16_t s_logo_scatter[LOGO_MAX_BLOCKS][2];
static uint16_t s_logo_delay_ms[LOGO_MAX_BLOCKS];
static uint8_t s_logo_tint[LOGO_MAX_BLOCKS]; // 0 amber, 1 grey, 2 white while in flight

static const float SPARK_X = 120, SPARK_Y = 110;

static void boot_tail(uint32_t e, uint32_t t0) {
    static const char maker[] = "KAFI DEVICES";
    if (e > t0) {
        char buf[sizeof(maker)];
        uint32_t n = (e - t0) / 30;
        if (n > sizeof(maker) - 1) n = sizeof(maker) - 1;
        for (uint32_t i = 0; i < n; i++) buf[i] = maker[i];
        buf[n] = '\0';
        text(buf, 120, 128, GREY, 1, CENTER);
    }
    if (e > t0 + 500) {
        uint32_t n = (e - t0 - 500) / 60;
        if (n > 12) n = 12;
        for (uint32_t i = 0; i < 12; i++) rect(72 + i * 8, 152, 7, 5, i < n ? WHITE : DARK);
    }
}

void fx_boot(uint32_t e) {
    if (e < 380) {
        int st = e / 95, s = 1 + st * 2;
        rect(SPARK_X - s / 2.0f, SPARK_Y - s / 2.0f, s, s, (st % 2) ? WHITE : AMBER);
        return;
    }
    if (e < 700) ring(SPARK_X, SPARK_Y, (e - 380) * 0.45f, e < 540 ? WHITE : GREY, 2);
    float k1 = ease_out((e - 380) / 420.0f);
    for (int i = 0; i < s_logo_n; i++) {
        float sx = s_logo_scatter[i][0], sy = s_logo_scatter[i][1];
        float k2 = ease_in_out(((float)e - 1000.0f - s_logo_delay_ms[i]) / 480.0f);
        float x, y;
        if (e < 1000) {
            x = lerpf(SPARK_X, sx, k1);
            y = lerpf(SPARK_Y, sy, k1);
        } else {
            x = lerpf(sx, s_logo[i][0], k2);
            y = lerpf(sy, s_logo[i][1], k2);
        }
        bool home = e >= 1000 && k2 >= 1.0f;
        uint32_t col = home ? (e < 1900 ? AMBER : WHITE)
                     : s_logo_tint[i] == 0 ? AMBER : s_logo_tint[i] == 1 ? GREY : WHITE;
        rect(x, y, 3, 3, col);
    }
    boot_tail(e, 1950);
}

// --- attract: Plasma radiating from the QUADRA lettering (or the active app's icon) ---
// The field's travelling term is keyed to each cell's distance from the centre shape (not
// from the panel center), so crests ripple outward from it; a glow term makes the plasma
// brightest next to the shape and fade toward the rim. Mostly black by design.
// In APP mode the shape is the active profile's 48x48 icon at 2x instead of the word.

#define PLASMA_CELL 4
#define PLASMA_N (240 / PLASMA_CELL)
#define SIN_LUT 256
#define ATTRACT_WORD "QUADRA"
#define ATTRACT_WORD_SCALE 3
#define ATTRACT_WORD_Y 113 // cap top; scale 3 caps are 15px -> centered on the panel
#define ATTRACT_ICON 48
#define ATTRACT_ICON_SCALE 2 // 96x96, whole-pixel
#define ATTRACT_ICON_X (CX - ATTRACT_ICON * ATTRACT_ICON_SCALE / 2)
#define ATTRACT_ICON_Y (CY - ATTRACT_ICON * ATTRACT_ICON_SCALE / 2)
static float s_sin[SIN_LUT];
static uint8_t s_plasma_mask[PLASMA_N * PLASMA_N / 8 + 1];
// px from the nearest cell of the centre shape, capped 255 -- one field per shape
static uint8_t s_dist_word[PLASMA_N * PLASMA_N];
static uint8_t s_dist_icon[PLASMA_N * PLASMA_N];
static const uint8_t *s_dist_icon_for = nullptr; // which icon s_dist_icon was built from
static float s_glow[256];                         // emission falloff by that distance

static inline float lut(float rad_as_units) { return s_sin[((int)rad_as_units) & (SIN_LUT - 1)]; }
static const float TO_UNITS = SIN_LUT / (2.0f * (float)M_PI);

static void plasma(uint32_t t, const uint8_t *dist) {
    static const uint32_t levels[5] = {BLACK, DARK, GREY, WHITE, AMBER};
    static const uint8_t bayer[4] = {0, 2, 3, 1};
    float T = t / 1000.0f;
    float p1 = T * 0.6f * TO_UNITS, p2 = -T * 0.8f * TO_UNITS, p4 = -T * 2.4f * TO_UNITS;
    for (int gy = 0; gy < PLASMA_N; gy++) {
        float y = gy * PLASMA_CELL + 2;
        float sy = lut(y / 17.0f * TO_UNITS + p2 + 1024);
        for (int gx = 0; gx < PLASMA_N; gx++) {
            int idx = gy * PLASMA_N + gx;
            if (!(s_plasma_mask[idx >> 3] & (1 << (idx & 7)))) continue;
            uint8_t d = dist[idx];
            if (d < 5) continue; // black halo keeps the shape readable
            float x = gx * PLASMA_CELL + 2;
            // Outward ripple (phase grows with distance, falls with time) plus a slow drift.
            float ripple = lut(d / 9.0f * TO_UNITS + p4 + 1024);
            float drift = 0.5f * (lut(x / 23.0f * TO_UNITS + p1) + sy);
            float v = (ripple + drift + 2.0f) / 4.0f; // 0..1
            v = v * v;
            v = v * v * s_glow[d];
            float f = v * 3.999f;
            int li = (int)f;
            if (f - li > (bayer[(gx & 1) + 2 * (gy & 1)] + 0.5f) / 4.0f) li++;
            if (li > 3) li = 3;
            if (v >= 0.85f) li = 4;
            if (li > 0) rect(gx * PLASMA_CELL, gy * PLASMA_CELL, PLASMA_CELL, PLASMA_CELL, levels[li]);
        }
    }
}

// Distance field from a set of seed cells. Only seeds on the shape's outline are measured
// against (the nearest seed to an outside cell is always an outline one), which keeps this
// to a few ms -- cheap enough to build an icon's field when the attract screen starts.
#define FIELD_CELLS (PLASMA_N * PLASMA_N)
#define FIELD_MAX_EDGE 1024 // outline cells measured against (a 96px icon has a few hundred)
static uint8_t s_seed_bits[FIELD_CELLS / 8 + 1]; // scratch: which cells the shape covers
static inline bool seed_at(int idx) { return s_seed_bits[idx >> 3] & (1 << (idx & 7)); }

static void build_field(uint8_t *dist) {
    static int16_t edge[FIELD_MAX_EDGE]; // scratch, off the task stack
    int ne = 0;
    for (int gy = 0; gy < PLASMA_N; gy++) {
        for (int gx = 0; gx < PLASMA_N; gx++) {
            int idx = gy * PLASMA_N + gx;
            if (!seed_at(idx)) continue;
            bool outline = gx == 0 || gy == 0 || gx == PLASMA_N - 1 || gy == PLASMA_N - 1
                        || !seed_at(idx - 1) || !seed_at(idx + 1) || !seed_at(idx - PLASMA_N) || !seed_at(idx + PLASMA_N);
            if (outline && ne < FIELD_MAX_EDGE) edge[ne++] = (int16_t)idx;
        }
    }
    for (int idx = 0; idx < FIELD_CELLS; idx++) {
        if (seed_at(idx) || ne == 0) {
            dist[idx] = seed_at(idx) ? 0 : 255;
            continue;
        }
        int gx = idx % PLASMA_N, gy = idx / PLASMA_N;
        int best = 1 << 30;
        for (int k = 0; k < ne; k++) {
            int dx = gx - edge[k] % PLASMA_N, dy = gy - edge[k] / PLASMA_N;
            int d2 = dx * dx + dy * dy;
            if (d2 < best) best = d2;
        }
        float d = sqrtf((float)best) * PLASMA_CELL;
        dist[idx] = (uint8_t)(d > 255 ? 255 : d);
    }
}

static void build_icon_field(const uint8_t *icon) {
    memset(s_seed_bits, 0, sizeof(s_seed_bits));
    for (int j = 0; j < ATTRACT_ICON; j++) {
        for (int i = 0; i < ATTRACT_ICON; i++) {
            const uint8_t *px = icon + (j * ATTRACT_ICON + i) * 2;
            if (px[0] == 0 && px[1] == 0) continue; // black = transparent (image565 skips it)
            int sx = ATTRACT_ICON_X + i * ATTRACT_ICON_SCALE, sy = ATTRACT_ICON_Y + j * ATTRACT_ICON_SCALE;
            int idx = (sy / PLASMA_CELL) * PLASMA_N + sx / PLASMA_CELL;
            s_seed_bits[idx >> 3] |= (uint8_t)(1 << (idx & 7));
        }
    }
    build_field(s_dist_icon);
    s_dist_icon_for = icon;
}

void fx_attract(uint32_t t_ms, const uint8_t *icon48) {
    if (icon48 != nullptr) {
        if (icon48 != s_dist_icon_for) build_icon_field(icon48);
        plasma(t_ms, s_dist_icon);
        image565(ATTRACT_ICON_X, ATTRACT_ICON_Y, ATTRACT_ICON, ATTRACT_ICON, icon48, 1.0f, ATTRACT_ICON_SCALE);
    } else {
        plasma(t_ms, s_dist_word);
        text(ATTRACT_WORD, CX, ATTRACT_WORD_Y, WHITE, ATTRACT_WORD_SCALE, CENTER);
    }
}

void fx_init() {
    s_logo_n = text_blocks("QUADRA", 120, 96, 3, s_logo, LOGO_MAX_BLOCKS);
    for (int i = 0; i < s_logo_n; i++) {
        float a = rnd(i, 1) * 2.0f * (float)M_PI, d = 30 + rnd(i, 2) * 80;
        s_logo_scatter[i][0] = (int16_t)lroundf(SPARK_X + cosf(a) * d);
        s_logo_scatter[i][1] = (int16_t)lroundf(SPARK_Y + sinf(a) * d);
        s_logo_delay_ms[i] = (uint16_t)(rnd(i, 3) * 260);
        float r4 = rnd(i, 4);
        s_logo_tint[i] = r4 < 0.3f ? 0 : r4 < 0.55f ? 1 : 2;
    }

    for (int i = 0; i < SIN_LUT; i++) s_sin[i] = sinf(i * 2.0f * (float)M_PI / SIN_LUT);
    for (int d = 0; d < 256; d++) s_glow[d] = 1.25f * expf(-d / 34.0f) + 0.15f;
    static int16_t word[LOGO_MAX_BLOCKS][2]; // one-time scratch, kept off the task stack
    int nw = text_blocks(ATTRACT_WORD, CX, ATTRACT_WORD_Y, ATTRACT_WORD_SCALE, word, LOGO_MAX_BLOCKS);
    for (int gy = 0; gy < PLASMA_N; gy++) {
        for (int gx = 0; gx < PLASMA_N; gx++) {
            int idx = gy * PLASMA_N + gx;
            float x = gx * PLASMA_CELL + 2, y = gy * PLASMA_CELL + 2;
            if (in_circle(x, y, 118)) s_plasma_mask[idx >> 3] |= (uint8_t)(1 << (idx & 7));
            float best = 1e9f;
            for (int k = 0; k < nw; k++) {
                float dx = x - (word[k][0] + ATTRACT_WORD_SCALE / 2.0f), dy = y - (word[k][1] + ATTRACT_WORD_SCALE / 2.0f);
                float d2 = dx * dx + dy * dy;
                if (d2 < best) best = d2;
            }
            float d = sqrtf(best);
            s_dist_word[idx] = (uint8_t)(d > 255 ? 255 : d);
        }
    }
}

} // namespace ui
