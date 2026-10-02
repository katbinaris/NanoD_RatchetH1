#include "media.h"
#include "app_mode.h"
#include "menu.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "media";

static media_track_t s_track;
static bool s_has_track = false;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static _Atomic uint32_t s_version = 0;

// The cover: an upload assembles in `staging` (TinyUSB task only); a verified one is swapped
// into `current` under the mutex. Both live in PSRAM, allocated on the first upload.
static uint8_t *s_staging = NULL, *s_current = NULL;
static size_t s_current_len = 0;
static uint32_t s_rx_len, s_rx_crc, s_rx_got;
static bool s_rx_active = false;
static SemaphoreHandle_t s_cover_lock = NULL;
static _Atomic uint32_t s_cover_version = 0;

static uint32_t crc32_ieee(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

void media_set_track(const media_track_t *t) {
    portENTER_CRITICAL(&s_mux);
    s_track = *t;
    s_track.title[MEDIA_TEXT_MAX] = '\0';
    s_track.artist[MEDIA_TEXT_MAX] = '\0';
    s_has_track = true;
    portEXIT_CRITICAL(&s_mux);
    atomic_fetch_add(&s_version, 1);
}

void media_clear(void) {
    portENTER_CRITICAL(&s_mux);
    s_has_track = false;
    portEXIT_CRITICAL(&s_mux);
    atomic_fetch_add(&s_version, 1);
}

bool media_get_track(media_track_t *out) {
    portENTER_CRITICAL(&s_mux);
    bool has = s_has_track;
    if (has) *out = s_track;
    portEXIT_CRITICAL(&s_mux);
    return has;
}

uint32_t media_version(void) { return atomic_load(&s_version); }

// What one volume key does, in %: on a Mac a quarter of its 16 steps under Shift+Option (music.c),
// a whole one without; on a PC (the modifier is dropped) Windows' 2. The host's report is the
// truth again once the knob has rested this long (it lags the keys by a USB round trip, so it
// isn't while they're coming).
#define VOL_FINE_MAC (100.0f / 64)
#define VOL_PLAIN_MAC (100.0f / 16)
#define VOL_STEP_PC 2.0f
#define VOL_SETTLE_US 300000
#define VOL_SHOW_US 1500000 // the ring stays up this long after the last key, fading in the last quarter
static float s_vol_shown = -1;
static int32_t s_vol_fine_seen = 0, s_vol_plain_seen = 0;
static int64_t s_vol_key_us = -(1LL << 40);

int media_volume(int64_t now_us, float *visible) {
    int32_t fine, plain;
    app_mode_volume_steps(&fine, &plain);
    const bool mac = menu_get_host() == MENU_HOST_MAC;
    portENTER_CRITICAL(&s_mux);
    int host = s_has_track ? s_track.volume : -1;
    int32_t df = fine - s_vol_fine_seen, dp = plain - s_vol_plain_seen;
    s_vol_fine_seen = fine;
    s_vol_plain_seen = plain;
    if (df || dp) s_vol_key_us = now_us;
    int64_t since = now_us - s_vol_key_us;
    float delta = mac ? df * VOL_FINE_MAC + dp * VOL_PLAIN_MAC : (df + dp) * VOL_STEP_PC;
    if (host >= 0 && (s_vol_shown < 0 || since > VOL_SETTLE_US)) s_vol_shown = host;
    else if (s_vol_shown >= 0) s_vol_shown = fminf(100.0f, fmaxf(0.0f, s_vol_shown + delta));
    float v = s_vol_shown;
    portEXIT_CRITICAL(&s_mux);
    float k = 1.0f - (float)since / VOL_SHOW_US;
    *visible = k <= 0 ? 0.0f : k > 0.25f ? 1.0f : k * 4;
    return v < 0 ? -1 : (int)lroundf(v);
}
uint32_t media_cover_version(void) { return atomic_load(&s_cover_version); }

bool media_cover_begin(uint32_t len, uint32_t crc) {
    if (s_cover_lock == NULL) s_cover_lock = xSemaphoreCreateMutex();
    if (len == 0 || len > MEDIA_COVER_MAX) return false;
    if (s_staging == NULL) s_staging = heap_caps_malloc(MEDIA_COVER_MAX, MALLOC_CAP_SPIRAM);
    if (s_current == NULL) s_current = heap_caps_malloc(MEDIA_COVER_MAX, MALLOC_CAP_SPIRAM);
    if (s_staging == NULL || s_current == NULL || s_cover_lock == NULL) {
        ESP_LOGW(TAG, "no PSRAM for the cover");
        return false;
    }
    s_rx_len = len;
    s_rx_crc = crc;
    s_rx_got = 0;
    s_rx_active = true;
    return true;
}

bool media_cover_data(uint32_t offset, const uint8_t *p, uint32_t n) {
    if (!s_rx_active || offset != s_rx_got || n == 0 || offset + n > s_rx_len) {
        s_rx_active = false; // a dropped or reordered piece: the whole transfer is void
        return false;
    }
    memcpy(s_staging + offset, p, n);
    s_rx_got += n;
    return true;
}

bool media_cover_end(void) {
    bool ok = s_rx_active && s_rx_got == s_rx_len && crc32_ieee(s_staging, s_rx_len) == s_rx_crc;
    s_rx_active = false;
    if (!ok) {
        ESP_LOGW(TAG, "cover rejected (%lu/%lu bytes)", (unsigned long)s_rx_got, (unsigned long)s_rx_len);
        return false;
    }
    xSemaphoreTake(s_cover_lock, portMAX_DELAY);
    uint8_t *t = s_current; // swap: the verified upload becomes current
    s_current = s_staging;
    s_staging = t;
    s_current_len = s_rx_len;
    xSemaphoreGive(s_cover_lock);
    atomic_fetch_add(&s_cover_version, 1);
    atomic_fetch_add(&s_version, 1);
    return true;
}

size_t media_cover_copy(uint8_t *dst, size_t cap, uint32_t *version) {
    if (s_cover_lock == NULL) return 0;
    xSemaphoreTake(s_cover_lock, portMAX_DELAY);
    size_t n = s_current_len <= cap ? s_current_len : 0;
    if (n) memcpy(dst, s_current, n);
    *version = atomic_load(&s_cover_version);
    xSemaphoreGive(s_cover_lock);
    return n;
}
