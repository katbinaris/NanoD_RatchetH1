#include "ui_shape.hpp"
#include "ui_gfx.hpp"
#include <math.h>
#include <stdlib.h>

namespace ui {

// --- meshes: vertices + faces (up to 4 corners; -1 = unused) ---

struct Mesh {
    const float (*v)[3];
    int nv;
    const int8_t (*f)[4];
    int nf;
};

static const float CUBE_V[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                                   {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
static const int8_t CUBE_F[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
static const float PYR_V[5][3] = {{-1, -0.8f, -1}, {1, -0.8f, -1}, {1, -0.8f, 1}, {-1, -0.8f, 1}, {0, 1.2f, 0}};
static const int8_t PYR_F[5][4] = {{0, 1, 2, 3}, {0, 1, 4, -1}, {1, 2, 4, -1}, {2, 3, 4, -1}, {3, 0, 4, -1}};
static const float OCTA_V[6][3] = {{1.3f, 0, 0}, {-1.3f, 0, 0}, {0, 1.3f, 0}, {0, -1.3f, 0}, {0, 0, 1.3f}, {0, 0, -1.3f}};
static const int8_t OCTA_F[8][4] = {{0, 2, 4, -1}, {0, 4, 3, -1}, {0, 3, 5, -1}, {0, 5, 2, -1},
                                    {1, 4, 2, -1}, {1, 3, 4, -1}, {1, 5, 3, -1}, {1, 2, 5, -1}};
static const Mesh MESHES[3] = {{CUBE_V, 8, CUBE_F, 6}, {PYR_V, 5, PYR_F, 5}, {OCTA_V, 6, OCTA_F, 8}};

#define MAX_V 8
#define MAX_F 8
#define MAX_E 12

static int corners(const int8_t *f) {
    return f[3] < 0 ? 3 : 4;
}

// Edges with the (up to two) faces that share them, derived once per mesh.
struct Edge {
    uint8_t a, b;
    int8_t f0, f1;
};
static Edge s_edges[3][MAX_E];
static int s_edge_count[3] = {-1, -1, -1};

static int edges_of(int m, const Edge **out) {
    if (s_edge_count[m] < 0) {
        const Mesh &mesh = MESHES[m];
        int n = 0;
        for (int fi = 0; fi < mesh.nf; fi++) {
            int c = corners(mesh.f[fi]);
            for (int i = 0; i < c; i++) {
                int a = mesh.f[fi][i], b = mesh.f[fi][(i + 1) % c];
                int lo = a < b ? a : b, hi = a < b ? b : a;
                int k = 0;
                while (k < n && !(s_edges[m][k].a == lo && s_edges[m][k].b == hi)) k++;
                if (k == n) s_edges[m][n++] = {(uint8_t)lo, (uint8_t)hi, (int8_t)fi, -1};
                else s_edges[m][k].f1 = (int8_t)fi;
            }
        }
        s_edge_count[m] = n;
    }
    *out = s_edges[m];
    return s_edge_count[m];
}

// --- pixels (the preview's put / line / fillPoly) ---

static inline void put(float x, float y, uint32_t c) {
    rect((float)(int)x, (float)(int)y, 1, 1, c); // truncate, like the preview's `x | 0`
}

// dashed: 2 pixels on, 2 off. pen: an even 2px line -- a vertical pixel pair on shallow
// lines, a horizontal pair on steep ones, so the width is the same at every angle.
static void line(float fx0, float fy0, float fx1, float fy1, uint32_t c, bool dashed = false, bool pen = false) {
    int x0 = (int)lroundf(fx0), y0 = (int)lroundf(fy0), x1 = (int)lroundf(fx1), y1 = (int)lroundf(fy1);
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    bool shallow = dx >= -dy;
    int err = dx + dy, n = 0;
    for (;;) {
        if (!dashed || (n & 3) < 2) {
            if (pen) rect(x0, y0, shallow ? 1 : 2, shallow ? 2 : 1, c);
            else rect(x0, y0, 1, 1, c);
        }
        n++;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Convex polygon fill: FILL_SPARSE = one GREY pixel in four (even x, even y), FILL_CHECKER =
// GREY checkerboard ((x + y) even), FILL_SOLID = plain DARK.
enum Fill { FILL_SPARSE, FILL_CHECKER, FILL_SOLID };
static void fill_pattern(const float (*p)[2], int n, Fill mode) {
    bool sparse = mode == FILL_SPARSE;
    float y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        if (p[i][1] < y0) y0 = p[i][1];
        if (p[i][1] > y1) y1 = p[i][1];
    }
    for (int y = (int)floorf(y0); y <= (int)ceilf(y1); y++) {
        float py = y + 0.5f, lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < n; i++) {
            float ax = p[i][0], ay = p[i][1], bx = p[(i + 1) % n][0], by = p[(i + 1) % n][1];
            if ((ay <= py && by > py) || (by <= py && ay > py)) {
                float x = ax + (py - ay) / (by - ay) * (bx - ax);
                if (x < lo) lo = x;
                if (x > hi) hi = x;
            }
        }
        if (lo > hi || (sparse && (y & 1))) continue;
        for (int x = (int)ceilf(lo - 0.5f); x <= (int)floorf(hi - 0.5f); x++) {
            if (mode == FILL_SOLID) rect(x, y, 1, 1, DARK);
            else if (sparse ? !(x & 1) : !((x + y) & 1)) rect(x, y, 1, 1, GREY);
        }
    }
}

// --- projection: orthographic, 30 deg down (2:1 isometric at a 45 deg yaw) ---

// Positive = looking down from above, like isometric games. (The first previews had the sign
// flipped and showed the shape from below -- base visible, near corner high on screen.)
static const float PITCH = (float)M_PI / 6;
static const float ISO_K = 0.86f;

static inline uint32_t tone_color(float t) {
    return t >= 0.85f ? WHITE : t >= 0.5f ? GREY : DARK;
}

// "Selected face" style: grey edges; the top face gets a sparse fill and white edges; hidden
// edges are a faint dash. `detail` (the main copy only) adds the fill, top edges and dashes.
static void draw_shape(ShapeKind kind, ShapeStyle style, float cx, float cy, float size, float yaw, float tone,
                       bool detail, bool amber) {
    const Mesh &m = MESHES[kind];
    float cyw = cosf(yaw), syw = sinf(yaw), cp = cosf(PITCH), sp = sinf(PITCH);
    float view[MAX_V][3], scr[MAX_V][2];
    for (int i = 0; i < m.nv; i++) {
        float x = m.v[i][0], y = m.v[i][1], z = m.v[i][2];
        float x1 = x * cyw + z * syw, z1 = -x * syw + z * cyw;
        view[i][0] = x1;
        view[i][1] = y * cp - z1 * sp;
        view[i][2] = y * sp + z1 * cp;
        scr[i][0] = cx + view[i][0] * ISO_K * size;
        scr[i][1] = cy - view[i][1] * ISO_K * size;
    }
    bool vis[MAX_F];
    float fy[MAX_F], fz[MAX_F];
    int top = -1, near = -1;
    for (int fi = 0; fi < m.nf; fi++) {
        const int8_t *f = m.f[fi];
        int c = corners(f);
        const float *p0 = view[f[0]], *p1 = view[f[1]], *p2 = view[f[2]];
        float u[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        float w[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        float n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
        float fc[3] = {0, 0, 0};
        for (int i = 0; i < c; i++)
            for (int j = 0; j < 3; j++) fc[j] += view[f[i]][j] / c;
        if (n[0] * fc[0] + n[1] * fc[1] + n[2] * fc[2] < 0) { // make it point outward
            n[0] = -n[0];
            n[1] = -n[1];
            n[2] = -n[2];
        }
        float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        vis[fi] = len > 0 && n[2] / len > 1e-6f; // orthographic: faces the viewer
        fy[fi] = fc[1];
        fz[fi] = fc[2];
        if (vis[fi] && (top < 0 || fy[fi] > fy[top])) top = fi;
        if (vis[fi] && (near < 0 || fz[fi] > fz[near])) near = fi;
    }
    const Edge *edges;
    int ne = edges_of(kind, &edges);
    uint32_t edge_c = amber ? AMBER : tone_color(tone);

    if (style == STYLE_CAD_GRIPS || style == STYLE_THICK) {
        // R2C: checkered nearest face + dashed hidden edges, white edges, hollow grips on
        // every visible corner. R4A (thick): solid dark face and hidden edges, a 2px pen,
        // 6x6 grips. Extras only on the main copy (`detail`).
        bool thick = style == STYLE_THICK;
        if (detail) {
            if (near >= 0) {
                float poly[4][2];
                int c = corners(m.f[near]);
                for (int i = 0; i < c; i++) {
                    poly[i][0] = scr[m.f[near][i]][0];
                    poly[i][1] = scr[m.f[near][i]][1];
                }
                fill_pattern(poly, c, thick ? FILL_SOLID : FILL_CHECKER);
            }
            for (int k = 0; k < ne; k++) {
                const Edge &e = edges[k];
                if (!(vis[e.f0] || (e.f1 >= 0 && vis[e.f1]))) line(scr[e.a][0], scr[e.a][1], scr[e.b][0], scr[e.b][1], DARK, !thick);
            }
        }
        bool front_v[MAX_V] = {};
        for (int k = 0; k < ne; k++) {
            const Edge &e = edges[k];
            if (vis[e.f0] || (e.f1 >= 0 && vis[e.f1])) {
                line(scr[e.a][0], scr[e.a][1], scr[e.b][0], scr[e.b][1], edge_c, false, thick && detail);
                front_v[e.a] = front_v[e.b] = true;
            }
        }
        if (detail) {
            for (int i = 0; i < m.nv; i++) {
                if (!front_v[i]) continue;
                if (thick) {
                    rect(scr[i][0] - 3, scr[i][1] - 3, 6, 6, edge_c);
                    rect(scr[i][0] - 1, scr[i][1] - 1, 2, 2, BLACK);
                } else {
                    rect(scr[i][0] - 2, scr[i][1] - 2, 5, 5, edge_c);
                    rect(scr[i][0] - 1, scr[i][1] - 1, 3, 3, BLACK);
                }
            }
        }
        return;
    }
    uint32_t quiet = amber ? AMBER : tone >= 0.85f ? GREY : DARK;
    if (detail) {
        for (int k = 0; k < ne; k++) {
            const Edge &e = edges[k];
            bool front = vis[e.f0] || (e.f1 >= 0 && vis[e.f1]);
            if (front) continue;
            // a very faint dash: one pixel every 4 along the longer axis
            float ax = scr[e.a][0], ay = scr[e.a][1], bx = scr[e.b][0], by = scr[e.b][1];
            int n = (int)fmaxf(fabsf(bx - ax), fabsf(by - ay));
            for (int s = 0; s <= n; s += 4) put(ax + (bx - ax) * s / (n ? n : 1), ay + (by - ay) * s / (n ? n : 1), DARK);
        }
        if (top >= 0) {
            float poly[4][2];
            int c = corners(m.f[top]);
            for (int i = 0; i < c; i++) {
                poly[i][0] = scr[m.f[top][i]][0];
                poly[i][1] = scr[m.f[top][i]][1];
            }
            fill_pattern(poly, c, FILL_SPARSE);
        }
    }
    for (int k = 0; k < ne; k++) {
        const Edge &e = edges[k];
        if (vis[e.f0] || (e.f1 >= 0 && vis[e.f1])) line(scr[e.a][0], scr[e.a][1], scr[e.b][0], scr[e.b][1], quiet);
    }
    if (top >= 0 && detail) {
        const int8_t *f = m.f[top];
        int c = corners(f);
        for (int i = 0; i < c; i++) {
            int a = f[i], b = f[(i + 1) % c];
            line(scr[a][0], scr[a][1], scr[b][0], scr[b][1], edge_c);
        }
    }
}

// --- scenes ---

static const float SX = 120, SY = 90; // shape centre in the band

static void scene_zoom(const ShapeView &v) {
    // Nested copies, each twice the last; the zoom phase slides them along, so the loop is
    // seamless. Size sets the tone: small = dark, mid = white, big = grey.
    float frac = v.zoom - floorf(v.zoom);
    for (int i = -3; i <= 3; i++) {
        float s = 7.0f * powf(2.0f, i + frac);
        if (s < 3 || s > 70) continue;
        float tone = s < 6 ? 0.3f : s < 11 ? 0.6f : s <= 30 ? 1.0f : s <= 48 ? 0.6f : 0.3f;
        draw_shape(v.shape, v.style, SX, SY, s, v.yaw, tone, tone == 1.0f, v.flash);
    }
}

static void scene_orbit(const ShapeView &v) {
    // A dotted ground ring with a marker chained to the knob.
    const float rx = 36, ry = 9, gy = SY + 26;
    for (float a = 0; a < 2 * (float)M_PI; a += (float)M_PI / 24) put(SX + rx * cosf(a), gy + ry * sinf(a), GREY);
    float ma = v.yaw + (float)M_PI / 2;
    rect(SX + rx * cosf(ma) - 1, gy + ry * sinf(ma) - 1, 3, 3, AMBER);
    draw_shape(v.shape, v.style, SX, SY - 4, 19, v.yaw, 1.0f, true, v.flash);
}

static void scene_pan(const ShapeView &v) {
    // Three rows of floor dots, farther rows slower (parallax); a row of shapes; all wrap.
    static const struct { float y, sp, par; uint32_t c; } rows[3] = {
        {112, 12, 0.5f, DARK}, {118, 16, 0.75f, GREY}, {125, 22, 1.0f, GREY}};
    for (const auto &r : rows) {
        float off = fmodf(fmodf(v.pan * r.par, r.sp) + r.sp, r.sp);
        for (float x = off - r.sp; x < 240; x += r.sp) put(x, r.y, r.c);
    }
    const float SP = 64;
    float off = fmodf(fmodf(v.pan, SP) + SP, SP);
    for (float x = off - SP * 2; x < 240 + SP; x += SP) {
        float d = fabsf(x - SX);
        if (d > 110) continue;
        float tone = d < 26 ? 1.0f : d < 70 ? 0.6f : 0.3f;
        draw_shape(v.shape, v.style, x, SY, 14, v.yaw, tone, tone == 1.0f, v.flash);
    }
}

void shape_scene(const ShapeView &v) {
    clip(0, 55, 240, 73);
    switch (v.scene) {
        case SCENE_ORBIT: scene_orbit(v); break;
        case SCENE_PAN: scene_pan(v); break;
        default: scene_zoom(v); break;
    }
    unclip();
}

} // namespace ui
