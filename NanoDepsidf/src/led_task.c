#include "led_task.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "menu.h"
#include "ui_state.h"
#include "app_mode.h"
#include "app_colors.h"
#include "app_profiles/app_profiles.h"
#include "sysmon.h"
#include <math.h>
#include <string.h>

static const char *TAG = "led";

// The two WS2811 strips (DEVELOPMENT_PLAN.md "LEDs"): ring A, 60 around the knob; ring B, 8
// under the keys, two per key. Colours follow the active profile (app_colors.c: the same
// accents as the idle screen), amber outside APP mode and in the menu.
//
//   rest      a dim gradient of the app's three colours around the ring; keys dim in the same
//             gradient (F1 -> F4), a key with no action nearly off
//   knob      a bright spot follows the knob and pulses on every detent click; it fades back
//             into the gradient once the knob rests
//   wheel     the ring splits into one segment per entry (cancel first, dim white), the
//             chosen one bright
//   end stop  a short white flash of the whole ring
//   keys      a held key lights up at full level
//
// Calm by design: 30 fps, a few hundred float ops a frame, the bits go out by RMT (ring A over
// DMA). Runs above the display (PRIO_LED) so the display's back-to-back frames can't starve it;
// it sleeps between frames, so it never starves anything else.

#define LED_FPS 30
#define LED_MAX 0.20f        // never brighter than 20% (user): every level below is a share of this
#define LED_REST 0.30f       // the resting gradient
#define LED_KEY_OFF 0.06f    // a key with no action
#define LED_SPOT 0.8f        // the knob spot; a click pulses it to 1.0
#define LED_BUDGET_MA 250.0f // estimated draw cap (~20 mA per channel at full), whatever the pattern
#define SPOT_HOLD_US 1200000 // the spot stays this long after the knob last moved...
#define SPOT_FADE_US 500000  // ...then fades back into the gradient
#define PULSE_US 140000      // detent click pulse
#define FLASH_US 260000      // end-stop flash

// Ring geometry, to confirm on hardware: which LED sits at 12 o'clock with the screen upright,
// and whether indices run clockwise (+1) or anticlockwise (-1). The legacy firmware ran the
// knob backwards along the ring, hence -1. A screen rotation turns the ring by 15 per quarter.
#define RING_OFFSET 0
#define RING_DIR (-1)
// Keys: two LEDs each on ring B (legacy_fw hmi_thread.cpp).
static const uint8_t KEY_LED[4][2] = {{3, 4}, {2, 5}, {1, 6}, {0, 7}};

typedef struct { float r, g, b; } rgbf_t;
static rgbf_t s_ring[NANO_LED_A_NUM], s_keys[NANO_LED_B_NUM];
static led_strip_handle_t s_ring_h = NULL, s_keys_h = NULL;

static rgbf_t hexf(uint32_t c, float level) {
    return (rgbf_t){((c >> 16) & 0xFF) / 255.0f * level, ((c >> 8) & 0xFF) / 255.0f * level, (c & 0xFF) / 255.0f * level};
}
static rgbf_t mixf(rgbf_t a, rgbf_t b, float t) {
    return (rgbf_t){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static rgbf_t maxf(rgbf_t a, rgbf_t b) {
    return (rgbf_t){a.r > b.r ? a.r : b.r, a.g > b.g ? a.g : b.g, a.b > b.b ? a.b : b.b};
}
// Around the loop acc0 -> acc1 -> acc2 -> acc0, u in [0, 1).
static rgbf_t gradient(const uint32_t acc[3], float u, float level) {
    u -= floorf(u);
    float x = u * 3;
    int i = (int)x;
    if (i > 2) i = 2;
    return mixf(hexf(acc[i], level), hexf(acc[(i + 1) % 3], level), x - i);
}

// `mem` = RMT memory in symbols (0 = the driver's default). Without DMA the driver refills that
// memory from an interrupt as the frame goes out; a refill that comes late (Core 1 busy with the
// display and USB) corrupts the bits and the LEDs flicker. More memory = more slack per refill.
static bool strip_new(int gpio, int n, led_color_component_format_t fmt, bool dma, size_t mem, led_strip_handle_t *out) {
    led_strip_config_t cfg = {
        .strip_gpio_num = gpio,
        .max_leds = n,
        // WS2811 in its 800 kHz mode, as legacy_fw drove it (FastLED: 0.32 / 0.64 us highs).
        // led_strip's own WS2811 model is the 400 kHz mode; its SK6812 timing (0.3 / 0.6 us highs,
        // 1.2 us bits, 3 colours here) is the closest match.
        .led_model = LED_MODEL_SK6812,
        .color_component_format = fmt,
    };
    led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = mem,
        .flags.with_dma = dma,
    };
    esp_err_t err = led_strip_new_rmt_device(&cfg, &rmt, out);
    if (err != ESP_OK && mem) { // not that much free RMT memory: the default size
        rmt.mem_block_symbols = 0;
        err = led_strip_new_rmt_device(&cfg, &rmt, out);
    }
    if (err != ESP_OK && dma) { // no DMA channel left: plain RMT still works, with more refills
        rmt.flags.with_dma = false;
        err = led_strip_new_rmt_device(&cfg, &rmt, out);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip on GPIO%d failed: %s", gpio, esp_err_to_name(err));
        *out = NULL;
        return false;
    }
    return true;
}

// What each strip last showed: a strip is only sent again when a byte changes. A held key or a
// resting gradient isn't re-sent 30 times a second, so a transmission glitch has nothing to hit.
static uint8_t s_ring_sent[NANO_LED_A_NUM][3], s_keys_sent[NANO_LED_B_NUM][3];
static bool s_sent_valid = false;

static bool put_pixel(led_strip_handle_t h, uint8_t sent[3], int i, rgbf_t c, float k) {
    uint8_t v[3] = {(uint8_t)lroundf(c.r * k * 255), (uint8_t)lroundf(c.g * k * 255), (uint8_t)lroundf(c.b * k * 255)};
    if (s_sent_valid && !memcmp(v, sent, 3)) return false;
    memcpy(sent, v, 3);
    led_strip_set_pixel(h, i, v[0], v[1], v[2]);
    return true;
}

// Scale everything so the estimated draw stays under the budget, then send what changed.
static void flush(int rotation) {
    float sum = 0;
    for (int i = 0; i < NANO_LED_A_NUM; i++) sum += s_ring[i].r + s_ring[i].g + s_ring[i].b;
    for (int i = 0; i < NANO_LED_B_NUM; i++) sum += s_keys[i].r + s_keys[i].g + s_keys[i].b;
    float k = LED_MAX, ma = sum * LED_MAX * 20.0f;
    if (ma > LED_BUDGET_MA) k *= LED_BUDGET_MA / ma;
    sysmon_set_led_ma(ma > LED_BUDGET_MA ? LED_BUDGET_MA : ma);
    if (s_ring_h) {
        bool dirty = false;
        for (int p = 0; p < NANO_LED_A_NUM; p++) {
            int i = ((RING_OFFSET + RING_DIR * (p + 15 * rotation)) % NANO_LED_A_NUM + NANO_LED_A_NUM) % NANO_LED_A_NUM;
            dirty |= put_pixel(s_ring_h, s_ring_sent[i], i, s_ring[p], k);
        }
        if (dirty) led_strip_refresh(s_ring_h);
    }
    if (s_keys_h) {
        bool dirty = false;
        for (int i = 0; i < NANO_LED_B_NUM; i++) dirty |= put_pixel(s_keys_h, s_keys_sent[i], i, s_keys[i], k);
        if (dirty) led_strip_refresh(s_keys_h);
    }
    s_sent_valid = true;
}

static void led_task_fn(void *arg) {
    // Colour order per strip as in legacy_fw: ring A RGB, ring B GRB.
    // Ring A takes the one DMA-capable TX channel; the keys get 96 symbols (two memory blocks,
    // half their 192-bit frame) so a late refill still has a whole block in hand.
    strip_new(PIN_LED_A, NANO_LED_A_NUM, LED_STRIP_COLOR_COMPONENT_FMT_RGB, true, 0, &s_ring_h);
    strip_new(PIN_LED_B, NANO_LED_B_NUM, LED_STRIP_COLOR_COMPONENT_FMT_GRB, false, 96, &s_keys_h);
    ESP_LOGI(TAG, "led task on core %d, prio %d: ring %s, keys %s", xPortGetCoreID(), uxTaskPriorityGet(NULL),
             s_ring_h ? "ok" : "off", s_keys_h ? "ok" : "off");

    const app_profile_t *acc_for = NULL;
    int acc_hid = -1;
    uint32_t acc[3];
    static const uint32_t AMBERS[3] = {0xFFC94Du, 0xFFC94Du, 0xFFC94Du};
    int32_t last_angle = ui_state_get_knob_angle();
    uint32_t last_clicks = ui_state_get_clicks(), last_walls = ui_state_get_walls();
    int64_t moved_at = -(1LL << 40), click_at = -(1LL << 40), wall_at = -(1LL << 40);
    TickType_t wake = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000 / LED_FPS));
        const int64_t now = esp_timer_get_time();

        // What the device is doing (atomics only).
        const bool menu = menu_is_open(), app = menu_get_hid_type() == MENU_HID_APP, idle = ui_state_get_screensaver();
        const app_profile_t *p = app ? app_profiles_get(menu_get_app_profile()) : NULL;
        if (p != acc_for || (int)app != acc_hid) {
            if (p) app_accents(p->icon48, p->plasma_heat, acc);
            else memcpy(acc, AMBERS, sizeof(acc));
            acc_for = p;
            acc_hid = app;
        }
        const uint32_t *pal = menu ? AMBERS : acc;
        const uint8_t held = ui_state_get_buttons();
        const int32_t angle = ui_state_get_knob_angle();
        const uint32_t clicks = ui_state_get_clicks(), walls = ui_state_get_walls();
        if (angle != last_angle) { moved_at = now; last_angle = angle; }
        if (clicks != last_clicks) { click_at = now; last_clicks = clicks; }
        if (walls != last_walls) { wall_at = now; last_walls = walls; }
        int w_ring = 0, w_entry = 0;
        const bool wheel = app && !menu && p && app_mode_wheel(&w_ring, &w_entry) && w_ring < p->ring_count;

        // Ring: the resting gradient (drifting slowly and dimmer while idle), or the wheel.
        const float drift = idle ? (float)(now % 30000000) / 30000000.0f : 0;
        if (wheel) {
            const int n = p->rings[w_ring].count + 1; // + cancel
            for (int i = 0; i < NANO_LED_A_NUM; i++) {
                int seg = i * n / NANO_LED_A_NUM;
                bool gap = (i * n) % NANO_LED_A_NUM < n; // the first LED of each segment stays dark
                rgbf_t c = seg == w_entry ? hexf(pal[2], 1.0f) : seg == 0 ? hexf(0xFFFFFF, 0.2f) : hexf(pal[1], 0.35f);
                s_ring[i] = gap ? (rgbf_t){0, 0, 0} : c;
            }
        } else {
            for (int i = 0; i < NANO_LED_A_NUM; i++)
                s_ring[i] = gradient(pal, (float)i / NANO_LED_A_NUM + drift, idle ? LED_REST * 0.7f : LED_REST);
            // The knob spot: follows the knob, pulses on each click, fades out once it rests.
            int64_t since = now - moved_at;
            float vis = since < SPOT_HOLD_US ? 1 : since < SPOT_HOLD_US + SPOT_FADE_US ? 1 - (float)(since - SPOT_HOLD_US) / SPOT_FADE_US : 0;
            if (vis > 0 && !idle) {
                float pulse = now - click_at < PULSE_US ? 1 - (float)(now - click_at) / PULSE_US : 0;
                float pos = (float)angle / 10000.0f / (2 * (float)M_PI) * NANO_LED_A_NUM;
                int c = (int)lroundf(pos);
                rgbf_t spot = mixf(hexf(pal[2], 1), hexf(0xFFFFFF, 1), 0.35f);
                for (int d = -2; d <= 2; d++) {
                    float lv = d == 0 ? LED_SPOT + (1 - LED_SPOT) * pulse : (d == 1 || d == -1) ? 0.3f + 0.3f * pulse : 0.25f * pulse;
                    if (lv <= 0) continue;
                    int i = ((c + d) % NANO_LED_A_NUM + NANO_LED_A_NUM) % NANO_LED_A_NUM;
                    s_ring[i] = mixf(s_ring[i], maxf(s_ring[i], (rgbf_t){spot.r * lv, spot.g * lv, spot.b * lv}), vis);
                }
            }
        }
        // End stop: the whole ring flashes white.
        if (now - wall_at < FLASH_US) {
            float f = 1 - (float)(now - wall_at) / FLASH_US;
            for (int i = 0; i < NANO_LED_A_NUM; i++) s_ring[i] = mixf(s_ring[i], (rgbf_t){1, 1, 1}, f * 0.8f);
        }

        // Keys: the same gradient F1 -> F4, full on while held, nearly off with no action.
        for (int k = 0; k < 4; k++) {
            float lv = LED_REST;
            if (held & (1u << k)) lv = 1.0f;
            else if (app && !menu && p && k + 1 != APP_SLOT_F4 && p->slot[k + 1].kind == APP_ACT_NONE) lv = LED_KEY_OFF;
            else if (idle) lv = LED_REST * 0.7f;
            rgbf_t c = gradient(pal, k / 4.0f, lv);
            s_keys[KEY_LED[k][0]] = c;
            s_keys[KEY_LED[k][1]] = c;
        }
        // Every 2 s, send everything once anyway: a frame a glitch did corrupt doesn't stay.
        static int s_frames = 0;
        if (++s_frames >= 2 * LED_FPS) { s_frames = 0; s_sent_valid = false; }
        flush((int)menu_get_display_rotation());
    }
}

void led_task_start(void) {
    xTaskCreatePinnedToCore(led_task_fn, "led", 4096, NULL, PRIO_LED, NULL, CORE_IO);
}
