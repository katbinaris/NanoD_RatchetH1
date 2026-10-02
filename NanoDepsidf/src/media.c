#include "media.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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
