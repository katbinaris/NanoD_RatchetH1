#include "screen_stream.h"
#include "ext_proto.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <string.h>

#define TILE 16
#define TILES_X (SCREEN_W / TILE)
#define TILES (TILES_X * (SCREEN_H / TILE)) // 225: a tile number fits a byte
#define TILE_BYTES (TILE * TILE * 2)
#define BUF_MAX (TILES * (4 + TILE_BYTES))
#define CHUNK 60

static _Atomic uint8_t s_fps = 0;
static _Atomic bool s_restart = false; // a new start: the host has nothing yet

// The frame on its way. The display task fills it while IDLE and hands it over (READY); the
// host_link side sends it a report at a time and gives it back.
enum { IDLE, READY };
static _Atomic int s_state = IDLE;
static uint8_t *s_buf;         // PSRAM, BUF_MAX
static uint32_t s_len, s_off;  // s_off: host_link's
static uint8_t s_seq;
// Display task only: what the host has (PSRAM), and a frame that couldn't go when pushed.
static uint16_t *s_sent;
static bool s_pending, s_all;
static int64_t s_next_us;

void screen_stream_set(uint8_t fps) {
    if (fps > 30) fps = 30;
    if (fps && atomic_load(&s_fps) == 0) atomic_store(&s_restart, true);
    atomic_store(&s_fps, fps);
}

// One tile's record at `out`: RLE when that's shorter, raw otherwise. Returns its length.
static uint32_t encode_tile(const uint16_t *f, int t, uint8_t *out) {
    const int x0 = (t % TILES_X) * TILE, y0 = (t / TILES_X) * TILE;
    uint8_t *p = out + 4, *end = out + 4 + TILE_BYTES - 3;
    uint16_t px = 0;
    int run = 0;
    bool rle = true;
    for (int y = 0; y < TILE && rle; y++) {
        const uint16_t *row = f + (y0 + y) * SCREEN_W + x0;
        for (int x = 0; x < TILE; x++) {
            if (run && row[x] == px && run < 255) {
                run++;
                continue;
            }
            if (run) {
                if (p > end) { // no shorter than raw
                    rle = false;
                    break;
                }
                p[0] = (uint8_t)run;
                memcpy(p + 1, &px, 2); // the frame's bytes are already big-endian RGB565
                p += 3;
            }
            px = row[x];
            run = 1;
        }
    }
    if (rle && p <= end) {
        p[0] = (uint8_t)run;
        memcpy(p + 1, &px, 2);
        p += 3;
    } else {
        rle = false;
        p = out + 4;
        for (int y = 0; y < TILE; y++, p += TILE * 2) memcpy(p, f + (y0 + y) * SCREEN_W + x0, TILE * 2);
    }
    uint32_t len = (uint32_t)(p - out - 4);
    out[0] = (uint8_t)t;
    out[1] = rle ? 1 : 0;
    out[2] = (uint8_t)len;
    out[3] = (uint8_t)(len >> 8);
    return len + 4;
}

void screen_stream_frame(const uint16_t *frame, bool fresh) {
    if (fresh) s_pending = true;
    uint8_t fps = atomic_load(&s_fps);
    if (fps == 0 || frame == NULL) return;
    if (atomic_exchange(&s_restart, false)) s_all = s_pending = true;
    if (!s_pending || atomic_load(&s_state) != IDLE) return;
    int64_t now = esp_timer_get_time();
    if (now < s_next_us) return;
    if (s_buf == NULL) {
        s_buf = heap_caps_malloc(BUF_MAX, MALLOC_CAP_SPIRAM);
        s_sent = heap_caps_malloc(SCREEN_W * SCREEN_H * 2, MALLOC_CAP_SPIRAM);
        if (s_buf == NULL || s_sent == NULL) {
            heap_caps_free(s_buf);
            heap_caps_free(s_sent);
            s_buf = NULL;
            s_sent = NULL;
            atomic_store(&s_fps, 0); // no room: no stream
            return;
        }
        s_all = true;
    }
    uint32_t n = 0;
    for (int t = 0; t < TILES; t++) {
        const int x0 = (t % TILES_X) * TILE, y0 = (t / TILES_X) * TILE;
        bool same = !s_all;
        for (int y = 0; y < TILE && same; y++)
            same = memcmp(frame + (y0 + y) * SCREEN_W + x0, s_sent + (y0 + y) * SCREEN_W + x0, TILE * 2) == 0;
        if (same) continue;
        for (int y = 0; y < TILE; y++)
            memcpy(s_sent + (y0 + y) * SCREEN_W + x0, frame + (y0 + y) * SCREEN_W + x0, TILE * 2);
        n += encode_tile(frame, t, s_buf + n);
    }
    s_pending = s_all = false;
    s_next_us = now + 1000000 / fps;
    if (n == 0) return; // the frame was the same as the last one sent
    s_len = n;
    s_off = 0;
    s_seq++;
    atomic_store(&s_state, READY);
}

bool screen_stream_next(uint8_t r[64]) {
    if (atomic_load(&s_state) != READY) return false;
    uint32_t n = s_len - s_off < CHUNK ? s_len - s_off : CHUNK;
    memset(r, 0, 64);
    r[0] = EXT_TAG_SCREEN;
    r[1] = s_seq;
    r[2] = (s_off == 0 ? 1 : 0) | (s_off + n >= s_len ? 2 : 0);
    r[3] = (uint8_t)n;
    memcpy(r + 4, s_buf + s_off, n);
    return true;
}

void screen_stream_sent(void) {
    uint32_t n = s_len - s_off < CHUNK ? s_len - s_off : CHUNK;
    s_off += n;
    if (s_off >= s_len) atomic_store(&s_state, IDLE);
}

void screen_stream_stop(void) {
    atomic_store(&s_fps, 0);
    atomic_store(&s_state, IDLE);
}
