#include "host_link.h"
#include "host_proto.h"
#include "icon_store.h"
#include "menu.h"
#include "sysmon.h"
#include "pd_status.h"
#include "ui_state.h"
#include "app_mode.h"
#include "led_task.h"
#include "app_profiles/app_profiles.h"
#include "app_profiles/profile_json.h"
#include "esp_heap_caps.h"
#include "class/hid/hid_device.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "host";

// Replies are built in the TinyUSB task (where OUT reports arrive) but sent from the usb task,
// together with the stream, so the two never race for the one IN endpoint.
#define REPLY_QUEUE_DEPTH 8
static QueueHandle_t s_replies;
static uint8_t s_instance;

static _Atomic uint8_t s_stream_hz = 0;

// --- Profile transfers ---
// Download: HOST_CMD_PROFILE_READ asks (TinyUSB task), the usb task serializes the profile and
// the pieces go out back to back -- each finished IN report sends the next one straight from
// TinyUSB's completion callback, so a 40KB profile takes well under a second instead of one
// piece per usb-task pass. s_tx_lock keeps the two senders from building the same piece.
static SemaphoreHandle_t s_tx_lock;
static _Atomic int s_read_req = -1;
static char *s_out = NULL;      // the JSON being sent (s_tx_lock)
static uint32_t s_out_len, s_out_off;
static bool s_out_began;
static uint8_t s_out_index;
// Upload: BEGIN / DATA land in the TinyUSB task (copy only); END hands the text to the usb
// task, which parses, applies and stores it.
static char *s_in = NULL;
static uint32_t s_in_len, s_in_crc, s_in_got;
static uint8_t s_in_flags;
static bool s_in_bad;
static _Atomic bool s_in_done = false;
// HOST_CMD_PROFILE_OP, done in the usb task (file I/O): bit 31 pending, 8-15 index, 0-7 op.
static _Atomic uint32_t s_op = 0;

// LEDs: a snapshot every LED_FRAME_US while streaming, sent as LED_PARTS reports back to back
// (from the completion callback, like a profile download). s_tx_lock.
#define LED_FRAME_US 66000
#define LED_PER_REPORT 20
#define LED_PARTS ((LED_VIEW_COUNT + LED_PER_REPORT - 1) / LED_PER_REPORT)
static uint8_t s_leds[LED_VIEW_COUNT][3];
static int s_led_part = LED_PARTS; // LED_PARTS = nothing to send

// CRC-32 (IEEE, reflected, zlib.crc32), as icon_store.c.
static uint32_t crc32_ieee(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

static void put_u16(uint8_t *b, uint16_t v) { memcpy(b, &v, 2); }
static void put_u32(uint8_t *b, uint32_t v) { memcpy(b, &v, 4); }
static void put_i32(uint8_t *b, int32_t v) { memcpy(b, &v, 4); }
static void put_f32(uint8_t *b, float v) { memcpy(b, &v, 4); }
// NUL-padded into an n-byte field (the reply buffer is zeroed, so the last byte stays 0).
static void put_str(uint8_t *b, const char *s, size_t n) {
    size_t len = s ? strnlen(s, n - 1) : 0;
    memcpy(b, s, len);
}

static void queue_reply(const uint8_t *r) {
    if (xQueueSend(s_replies, r, 0) != pdTRUE) ESP_LOGW(TAG, "reply dropped (queue full)");
}

static void build_settings(uint8_t *r) {
    menu_remote_settings_t s;
    menu_remote_get(&s);
    r[0] = HOST_TAG_SETTINGS;
    put_u16(r + 1, s.dirty);
    put_i32(r + 4, s.detents);
    put_f32(r + 8, s.kp);
    put_f32(r + 12, s.kd);
    r[16] = (uint8_t)s.feel;
    r[17] = (uint8_t)s.amp;
    put_f32(r + 18, s.pitch);
    r[22] = (uint8_t)s.sound;
    r[23] = (uint8_t)s.hid_type;
    r[24] = (uint8_t)s.midi_channel;
    r[25] = (uint8_t)s.profile;
    r[26] = (uint8_t)s.boot_mode;
    r[27] = (uint8_t)s.rotation;
    r[28] = (uint8_t)s.host;
}

static void result_reply(uint8_t *r, uint8_t cmd, uint8_t res, int index, bool removed, const char *why) {
    memset(r, 0, HOST_REPORT_SIZE);
    r[0] = HOST_TAG_RESULT;
    r[1] = cmd;
    r[2] = res;
    r[3] = (uint8_t)(index < 0 ? 0 : index);
    r[4] = (uint8_t)app_profiles_count();
    r[5] = removed;
    if (why) put_str(r + 8, why, HOST_REPORT_SIZE - 8);
}

static void error_reply(uint8_t *r, uint8_t cmd, uint8_t err) {
    r[0] = HOST_TAG_ERROR;
    r[1] = cmd;
    r[2] = err;
}

// One command -> at most one reply in `r`. Returns false when there's nothing to send.
static bool handle(const uint8_t *in, uint8_t *r) {
    uint8_t cmd = in[0];
    switch (cmd) {
        case HOST_CMD_HELLO: {
            const esp_app_desc_t *app = esp_app_get_description();
            r[0] = HOST_TAG_HELLO;
            r[1] = HOST_PROTO_VERSION;
            r[2] = (uint8_t)app_profiles_count();
            r[3] = ui_state_get_usb_serial_active() ? 1 : 0;
            put_str(r + 4, app->version, 32);
            put_str(r + 36, app->date, 16);
            return true;
        }
        case HOST_CMD_GET_SETTINGS:
            build_settings(r);
            return true;
        case HOST_CMD_SET: {
            int32_t iv;
            float fv;
            memcpy(&iv, in + 4, 4);
            memcpy(&fv, in + 4, 4);
            if (!menu_remote_set(in[1], iv, fv)) {
                error_reply(r, cmd, HOST_ERR_BAD_PARAM);
                return true;
            }
            build_settings(r);
            return true;
        }
        case HOST_CMD_SAVE:
            menu_remote_save();
            build_settings(r);
            return true;
        case HOST_CMD_REVERT:
            menu_remote_revert();
            build_settings(r);
            return true;
        case HOST_CMD_RESET_PEAKS:
            sysmon_reset_peaks();
            return false;
        case HOST_CMD_STREAM:
            atomic_store(&s_stream_hz, in[1] > 50 ? 50 : in[1]);
            return false;
        case HOST_CMD_PROFILE: {
            if (in[1] >= app_profiles_count()) {
                error_reply(r, cmd, HOST_ERR_BAD_PARAM);
                return true;
            }
            const app_profile_t *p = app_profiles_get(in[1]);
            r[0] = HOST_TAG_PROFILE;
            r[1] = in[1];
            r[2] = (uint8_t)app_profiles_count();
            r[3] = (p->icon48 != NULL) | (uint8_t)(app_profiles_flags(in[1]) << 1);
            put_str(r + 4, p->id, 12);
            put_str(r + 16, p->name, 16);
            for (int i = 0; i < 4; i++) put_str(r + 32 + i * 8, p->legend[i], 8);
            return true;
        }
        case HOST_CMD_PROFILE_ICON: {
            uint16_t off;
            memcpy(&off, in + 2, 2);
            const app_profile_t *p = in[1] < app_profiles_count() ? app_profiles_get(in[1]) : NULL;
            if (p == NULL || p->icon48 == NULL || off >= ICON_BYTES) {
                error_reply(r, cmd, HOST_ERR_BAD_PARAM);
                return true;
            }
            uint16_t len = ICON_BYTES - off < HOST_ICON_CHUNK ? ICON_BYTES - off : HOST_ICON_CHUNK;
            r[0] = HOST_TAG_PROFILE_ICON;
            r[1] = in[1];
            put_u16(r + 2, off);
            r[4] = (uint8_t)len;
            memcpy(r + 8, p->icon48 + off, len);
            return true;
        }
        case HOST_CMD_PROFILE_READ:
            if (in[1] >= app_profiles_count()) {
                error_reply(r, cmd, HOST_ERR_BAD_PARAM);
                return true;
            }
            atomic_store(&s_read_req, in[1]);
            return false;
        case HOST_CMD_UPLOAD_BEGIN: {
            uint32_t len, crc;
            memcpy(&len, in + 4, 4);
            memcpy(&crc, in + 8, 4);
            if (atomic_load(&s_in_done)) {
                result_reply(r, cmd, HOST_RES_BUSY, 0, false, NULL);
                return true;
            }
            free(s_in);
            s_in = NULL;
            if (len == 0 || len > HOST_PROFILE_MAX_BYTES ||
                (s_in = heap_caps_malloc_prefer(len + 1, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT)) == NULL) {
                result_reply(r, cmd, HOST_RES_FULL, 0, false, "too big");
                return true;
            }
            s_in_len = len;
            s_in_crc = crc;
            s_in_got = 0;
            s_in_flags = in[1];
            s_in_bad = false;
            return false;
        }
        case HOST_CMD_UPLOAD_DATA: {
            uint32_t off = in[1] | in[2] << 8 | (uint32_t)in[3] << 16;
            if (s_in == NULL || atomic_load(&s_in_done) || off != s_in_got) {
                s_in_bad = true; // reported at END
                return false;
            }
            uint32_t n = s_in_len - off < HOST_TEXT_CHUNK ? s_in_len - off : HOST_TEXT_CHUNK;
            memcpy(s_in + off, in + 4, n);
            s_in_got += n;
            return false;
        }
        case HOST_CMD_UPLOAD_END:
            if (s_in == NULL) {
                result_reply(r, cmd, HOST_RES_TRANSFER, 0, false, "no upload");
                return true;
            }
            atomic_store(&s_in_done, true); // the usb task takes it from here
            return false;
        case HOST_CMD_PROFILE_OP:
            if (atomic_load(&s_op)) {
                result_reply(r, cmd, HOST_RES_BUSY, 0, false, NULL);
                return true;
            }
            atomic_store(&s_op, 0x80000000u | (uint32_t)in[1] << 8 | in[2]);
            return false;
        default:
            error_reply(r, cmd, HOST_ERR_UNKNOWN_CMD);
            return true;
    }
}

void host_link_init(uint8_t vendor_instance) {
    s_instance = vendor_instance;
    s_replies = xQueueCreate(REPLY_QUEUE_DEPTH, HOST_REPORT_SIZE);
    s_tx_lock = xSemaphoreCreateMutex();
}

void host_link_handle_report(const uint8_t *report, uint16_t len) {
    uint8_t in[HOST_REPORT_SIZE] = {0};
    uint8_t r[HOST_REPORT_SIZE] = {0};
    memcpy(in, report, len < sizeof(in) ? len : sizeof(in));
    if (in[0] >= 0x10 && in[0] <= 0x1F) {
        if (handle(in, r)) queue_reply(r);
    } else if (icon_store_handle_report(in, sizeof(in), r)) {
        queue_reply(r);
    }
}

static void build_state(uint8_t *r, uint16_t seq) {
    r[0] = HOST_TAG_STATE;
    put_u16(r + 1, seq);
    put_i32(r + 4, ui_state_get_knob_angle());
    put_i32(r + 8, ui_state_get_detent());
    r[12] = ui_state_get_buttons();
    r[13] = (uint8_t)menu_current_screen();
    r[14] = ui_state_get_screensaver();
    r[15] = (uint8_t)app_mode_live_slot();
    put_i32(r + 16, (int32_t)ui_state_get_clicks());
    put_i32(r + 20, (int32_t)ui_state_get_walls());
}

static void build_sys(uint8_t *a, uint8_t *b) {
    sysmon_info_t s;
    sysmon_get(&s);
    pd_status_t pd = pd_status_get();
    a[0] = HOST_TAG_SYS_A;
    put_u16(a + 4, s.motor_ma);
    put_u16(a + 6, s.led_ma);
    put_u16(a + 8, s.board_ma);
    put_u16(a + 10, s.total_ma);
    put_u16(a + 12, s.total_peak_ma);
    a[14] = s.chip_ok;
    put_f32(a + 16, s.chip_c);
    put_f32(a + 20, s.chip_peak_c);
    put_u16(a + 24, s.coil_ma);
    put_u16(a + 26, s.coil_peak_ma);
    put_f32(a + 28, s.copper_w);
    a[32] = (uint8_t)pd.source;
    put_u16(a + 34, pd.ma);
    put_u16(a + 36, pd.mv);

    b[0] = HOST_TAG_SYS_B;
    b[4] = s.load[0];
    b[5] = s.load[1];
    b[6] = s.load_peak[0];
    b[7] = s.load_peak[1];
    put_f32(b + 8, s.loop_khz);
    put_f32(b + 12, s.work_avg_us);
    put_f32(b + 16, s.work_max_us);
    put_f32(b + 20, s.jitter_max_us);
    put_u32(b + 24, s.missed);
    put_f32(b + 28, s.spikes_per_s);
    put_u32(b + 32, s.heap_free);
    put_u32(b + 36, s.heap_min);
    put_u32(b + 40, s.hid_drops);
    put_u32(b + 44, s.audio_gaps);
    put_u32(b + 48, s.uptime_s);
    put_u32(b + 52, s.sensor_crc_errors);
}

// The next piece of a profile download, if the endpoint is free. usb task and TinyUSB task.
static void send_piece(void) {
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    if (s_out != NULL && tud_hid_n_ready(s_instance)) {
        uint8_t r[HOST_REPORT_SIZE] = {0};
        uint32_t n = 0;
        if (!s_out_began) {
            r[0] = HOST_TAG_PROFILE_BEGIN;
            r[1] = s_out_index;
            put_u32(r + 4, s_out_len);
            put_u32(r + 8, crc32_ieee((const uint8_t *)s_out, s_out_len));
        } else {
            n = s_out_len - s_out_off < HOST_TEXT_CHUNK ? s_out_len - s_out_off : HOST_TEXT_CHUNK;
            r[0] = HOST_TAG_PROFILE_DATA;
            r[1] = s_out_off & 0xFF;
            r[2] = (s_out_off >> 8) & 0xFF;
            r[3] = (s_out_off >> 16) & 0xFF;
            memcpy(r + 4, s_out + s_out_off, n);
        }
        if (tud_hid_n_report(s_instance, 0, r, sizeof(r))) {
            if (!s_out_began) s_out_began = true;
            else s_out_off += n;
            if (s_out_began && s_out_off >= s_out_len) {
                free(s_out);
                s_out = NULL;
            }
        }
    }
    xSemaphoreGive(s_tx_lock);
}

// The next LED report of the current frame, if the endpoint is free. Caller holds s_tx_lock.
static void send_led_part_locked(void) {
    if (s_led_part >= LED_PARTS || !tud_hid_n_ready(s_instance)) return;
    int first = s_led_part * LED_PER_REPORT;
    int n = LED_VIEW_COUNT - first < LED_PER_REPORT ? LED_VIEW_COUNT - first : LED_PER_REPORT;
    uint8_t r[HOST_REPORT_SIZE] = {0};
    r[0] = HOST_TAG_LEDS;
    r[1] = (uint8_t)first;
    r[2] = (uint8_t)n;
    memcpy(r + 4, s_leds[first], (size_t)n * 3);
    if (tud_hid_n_report(s_instance, 0, r, sizeof(r))) s_led_part++;
}

void host_link_report_sent(void) {
    send_piece();
    if (atomic_load(&s_stream_hz) == 0) return;
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    if (s_out == NULL) send_led_part_locked();
    xSemaphoreGive(s_tx_lock);
}

static bool sending_profile(void) {
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    bool busy = s_out != NULL;
    xSemaphoreGive(s_tx_lock);
    return busy;
}

// usb task: the slow half of the profile commands (JSON, files).
static void profile_work(void) {
    uint8_t r[HOST_REPORT_SIZE];
    int want = atomic_exchange(&s_read_req, -1);
    if (want >= 0) {
        size_t len = 0;
        char *text = profile_json_write(app_profiles_get(want), &len);
        if (text == NULL) {
            memset(r, 0, sizeof(r));
            error_reply(r, HOST_CMD_PROFILE_READ, HOST_ERR_BAD_PARAM);
            queue_reply(r);
        } else {
            xSemaphoreTake(s_tx_lock, portMAX_DELAY);
            free(s_out); // a new request replaces one still going
            s_out = text;
            s_out_len = len;
            s_out_off = 0;
            s_out_began = false;
            s_out_index = (uint8_t)want;
            xSemaphoreGive(s_tx_lock);
        }
    }

    if (atomic_load(&s_in_done)) {
        char err[HOST_REPORT_SIZE - 8];
        int index = 0;
        bool removed = false;
        uint8_t res = HOST_RES_OK;
        if (s_in_bad || s_in_got != s_in_len || crc32_ieee((const uint8_t *)s_in, s_in_len) != s_in_crc) {
            res = HOST_RES_TRANSFER;
            snprintf(err, sizeof(err), "got %u of %u bytes%s", (unsigned)s_in_got, (unsigned)s_in_len,
                     s_in_bad ? ", out of order" : "");
        } else {
            s_in[s_in_len] = '\0';
            app_profile_t *p = profile_json_read(s_in, s_in_len, err, sizeof(err));
            if (p == NULL) {
                res = HOST_RES_INVALID;
            } else {
                app_profiles_err_t e = app_profiles_put_live(p, &index);
                if (e == APP_PROFILES_OK && (s_in_flags & HOST_UPLOAD_SAVE)) e = app_profiles_save(index);
                res = e == APP_PROFILES_OK ? HOST_RES_OK : e == APP_PROFILES_ERR_FULL ? HOST_RES_FULL : HOST_RES_STORAGE;
                snprintf(err, sizeof(err), "%s", res == HOST_RES_OK ? "" : "not stored");
            }
        }
        if (res != HOST_RES_OK) ESP_LOGW(TAG, "profile upload: %u (%s)", res, err);
        free(s_in);
        s_in = NULL;
        atomic_store(&s_in_done, false);
        result_reply(r, HOST_CMD_UPLOAD_END, res, index, removed, err);
        queue_reply(r);
    }

    uint32_t op = atomic_load(&s_op);
    if (op) {
        int index = (op >> 8) & 0xFF;
        bool removed = false;
        app_profiles_err_t e = APP_PROFILES_ERR_INDEX;
        switch (op & 0xFF) {
            case HOST_OP_SAVE: e = app_profiles_save(index); break;
            case HOST_OP_REVERT: e = app_profiles_revert(index, &removed); break;
            case HOST_OP_REMOVE: e = app_profiles_remove(index, &removed); break;
        }
        if (removed) menu_profile_removed(index);
        result_reply(r, HOST_CMD_PROFILE_OP,
                     e == APP_PROFILES_OK ? HOST_RES_OK : e == APP_PROFILES_ERR_STORAGE ? HOST_RES_STORAGE : HOST_RES_BAD_INDEX,
                     index, removed, NULL);
        queue_reply(r);
        atomic_store(&s_op, 0);
    }
    app_profiles_reap();
}

void host_link_poll(void) {
    profile_work();

    // Queued replies first, in order; one that can't go out yet waits for the next pass.
    uint8_t r[HOST_REPORT_SIZE];
    while (xQueuePeek(s_replies, r, 0) == pdTRUE) {
        xSemaphoreTake(s_tx_lock, portMAX_DELAY);
        bool sent = tud_hid_n_ready(s_instance) && tud_hid_n_report(s_instance, 0, r, sizeof(r));
        xSemaphoreGive(s_tx_lock);
        if (!sent) return;
        xQueueReceive(s_replies, r, 0);
    }
    // A profile download has the endpoint to itself (the live stream pauses meanwhile).
    if (sending_profile()) {
        send_piece();
        return;
    }

    uint8_t hz = atomic_load(&s_stream_hz);
    if (hz == 0) return;
    static int64_t s_next_state = 0, s_next_sys = 0;
    static uint16_t s_seq = 0;
    static uint8_t s_sys_b[HOST_REPORT_SIZE];
    static bool s_sys_b_pending = false;
    int64_t now = esp_timer_get_time();

    // SYS_B follows SYS_A on the next free slot (one report per ready endpoint).
    if (s_sys_b_pending && tud_hid_n_ready(s_instance)) {
        if (tud_hid_n_report(s_instance, 0, s_sys_b, sizeof(s_sys_b))) s_sys_b_pending = false;
        return;
    }
    if (now >= s_next_sys && tud_hid_n_ready(s_instance)) {
        uint8_t a[HOST_REPORT_SIZE] = {0};
        memset(s_sys_b, 0, sizeof(s_sys_b));
        build_sys(a, s_sys_b);
        if (tud_hid_n_report(s_instance, 0, a, sizeof(a))) {
            s_sys_b_pending = true;
            s_next_sys = now + 500000;
        }
        return;
    }
    // LEDs: a new frame once the last one is out; its first report now, the rest follow as
    // each one completes.
    static int64_t s_next_leds = 0;
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    if (s_led_part >= LED_PARTS && now >= s_next_leds) {
        led_task_snapshot(s_leds);
        s_led_part = 0;
        s_next_leds = now + LED_FRAME_US;
    }
    bool led_busy = s_led_part < LED_PARTS;
    if (led_busy) send_led_part_locked();
    xSemaphoreGive(s_tx_lock);
    if (led_busy) return;
    if (now >= s_next_state && tud_hid_n_ready(s_instance)) {
        uint8_t st[HOST_REPORT_SIZE] = {0};
        build_state(st, s_seq);
        if (tud_hid_n_report(s_instance, 0, st, sizeof(st))) {
            s_seq++;
            s_next_state = now + 1000000 / hz;
        }
    }
}

bool host_link_streaming(void) {
    return atomic_load(&s_stream_hz) != 0;
}

void host_link_stop(void) {
    atomic_store(&s_stream_hz, 0);
    xQueueReset(s_replies);
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    free(s_out);
    s_out = NULL;
    s_led_part = LED_PARTS;
    xSemaphoreGive(s_tx_lock);
    atomic_store(&s_read_req, -1);
    // An upload cut off mid-way: the next BEGIN starts over (and frees the old buffer).
}
