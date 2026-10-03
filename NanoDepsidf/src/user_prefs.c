#include "user_prefs.h"
#include "tasks_common.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "user_prefs";

#define NS "user_prefs"
#define KEY_TEXT "idle_text"
#define KEY_LIGHTS "lights"
#define LIGHTS_VERSION 1

// --- idle text ---
static char s_text[USER_TEXT_MAX + 1];
static portMUX_TYPE s_text_mux = portMUX_INITIALIZER_UNLOCKED;
static _Atomic uint32_t s_text_version = 1;

// --- LIGHTS: one atomic per field (rotated on Core 0, read by the LED task) ---
static _Atomic int32_t s_src = LIGHT_SRC_APP, s_hue = 30, s_sat = 100, s_fx = LIGHT_FX_GRADIENT,
                       s_speed = 4, s_level = 100;

typedef struct {
    uint32_t version;
    lights_t l;
} lights_blob_t;

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t CONTROL_HOT wrapi(int32_t v, int32_t n) { v %= n; return v < 0 ? v + n : v; }

static void sanitize(char *dst, const char *src) {
    size_t n = 0;
    for (; src[n] && n < USER_TEXT_MAX; n++) dst[n] = (src[n] >= 0x20 && src[n] <= 0x7E) ? src[n] : ' ';
    while (n > 0 && dst[n - 1] == ' ') n--; // trailing spaces would only shift the centring
    dst[n] = '\0';
}

void user_prefs_init(void) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return; // nothing saved yet: defaults
    char buf[USER_TEXT_MAX + 1];
    size_t len = sizeof(buf);
    if (nvs_get_str(h, KEY_TEXT, buf, &len) == ESP_OK) sanitize(s_text, buf);
    lights_blob_t b;
    len = sizeof(b);
    if (nvs_get_blob(h, KEY_LIGHTS, &b, &len) == ESP_OK && len == sizeof(b) && b.version == LIGHTS_VERSION) {
        lights_set(&b.l);
    }
    nvs_close(h);
}

static bool nvs_write(const char *key, const void *data, size_t size, bool is_str) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = is_str ? nvs_set_str(h, key, (const char *)data) : nvs_set_blob(h, key, data, size);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "saving %s failed: %s", key, esp_err_to_name(err));
    return err == ESP_OK;
}

uint32_t user_text_version(void) { return atomic_load(&s_text_version); }

void user_text_get(char *out, size_t n) {
    if (n == 0) return;
    portENTER_CRITICAL(&s_text_mux);
    strncpy(out, s_text, n - 1);
    portEXIT_CRITICAL(&s_text_mux);
    out[n - 1] = '\0';
}

bool user_text_set(const char *s) {
    char clean[USER_TEXT_MAX + 1];
    sanitize(clean, s);
    portENTER_CRITICAL(&s_text_mux);
    memcpy(s_text, clean, sizeof(clean));
    portEXIT_CRITICAL(&s_text_mux);
    atomic_fetch_add(&s_text_version, 1);
    return nvs_write(KEY_TEXT, clean, 0, true);
}

void lights_get(lights_t *out) {
    out->src = atomic_load(&s_src);
    out->hue = atomic_load(&s_hue);
    out->sat = atomic_load(&s_sat);
    out->fx = atomic_load(&s_fx);
    out->speed = atomic_load(&s_speed);
    out->level = atomic_load(&s_level);
}

void lights_set(const lights_t *in) {
    atomic_store(&s_src, clampi(in->src, 0, LIGHT_SRC_COUNT - 1));
    atomic_store(&s_hue, wrapi(in->hue, 360));
    atomic_store(&s_sat, clampi(in->sat, 0, 100));
    atomic_store(&s_fx, clampi(in->fx, 0, LIGHT_FX_COUNT - 1));
    atomic_store(&s_speed, clampi(in->speed, 1, 10));
    atomic_store(&s_level, clampi(in->level, 10, 200));
}

bool lights_save(void) {
    lights_blob_t b = {.version = LIGHTS_VERSION};
    lights_get(&b.l);
    return nvs_write(KEY_LIGHTS, &b, sizeof(b), false);
}

void CONTROL_HOT lights_rotate_src(int8_t dir) { atomic_store(&s_src, wrapi(atomic_load(&s_src) + dir, LIGHT_SRC_COUNT)); }
void CONTROL_HOT lights_rotate_hue(int8_t dir) { atomic_store(&s_hue, wrapi(atomic_load(&s_hue) + dir * LIGHT_HUE_STEP, 360)); }
void CONTROL_HOT lights_rotate_fx(int8_t dir) { atomic_store(&s_fx, wrapi(atomic_load(&s_fx) + dir, LIGHT_FX_COUNT)); }
void CONTROL_HOT lights_rotate_sat(int8_t dir) {
    int32_t v = atomic_load(&s_sat) + dir * LIGHT_SAT_STEP;
    atomic_store(&s_sat, v < 0 ? 0 : v > 100 ? 100 : v);
}
void CONTROL_HOT lights_rotate_speed(int8_t dir) {
    int32_t v = atomic_load(&s_speed) + dir;
    atomic_store(&s_speed, v < 1 ? 1 : v > 10 ? 10 : v);
}
void CONTROL_HOT lights_rotate_level(int8_t dir) {
    int32_t v = atomic_load(&s_level) + dir * LIGHT_LEVEL_STEP;
    atomic_store(&s_level, v < 10 ? 10 : v > 200 ? 200 : v);
}

bool CONTROL_HOT lights_custom(void) { return atomic_load(&s_src) == LIGHT_SRC_CUSTOM; }
bool CONTROL_HOT lights_fx_animated(void) {
    int32_t fx = atomic_load(&s_fx);
    return fx == LIGHT_FX_BREATHE || fx == LIGHT_FX_SPIN || fx == LIGHT_FX_RAINBOW;
}

const char *lights_fx_name(int32_t fx) {
    static const char *const NAMES[LIGHT_FX_COUNT] = {"GRADIENT", "SOLID", "BREATHE", "SPIN", "RAINBOW", "OFF"};
    return fx >= 0 && fx < LIGHT_FX_COUNT ? NAMES[fx] : "?";
}

uint32_t lights_hsv(float h, float s, float v) {
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = v - c, r, g, b;
    switch ((int)(h / 60.0f)) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    return (uint32_t)lroundf((r + m) * 255) << 16 | (uint32_t)lroundf((g + m) * 255) << 8 | (uint32_t)lroundf((b + m) * 255);
}
