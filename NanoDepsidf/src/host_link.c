#include "host_link.h"
#include "host_proto.h"
#include "icon_store.h"
#include "menu.h"
#include "sysmon.h"
#include "pd_status.h"
#include "ui_state.h"
#include "app_mode.h"
#include "app_profiles/app_profiles.h"
#include "class/hid/hid_device.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "host";

// Replies are built in the TinyUSB task (where OUT reports arrive) but sent from the usb task,
// together with the stream, so the two never race for the one IN endpoint.
#define REPLY_QUEUE_DEPTH 8
static QueueHandle_t s_replies;
static uint8_t s_instance;

static _Atomic uint8_t s_stream_hz = 0;

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
            r[3] = p->icon48 != NULL;
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
        default:
            error_reply(r, cmd, HOST_ERR_UNKNOWN_CMD);
            return true;
    }
}

void host_link_init(uint8_t vendor_instance) {
    s_instance = vendor_instance;
    s_replies = xQueueCreate(REPLY_QUEUE_DEPTH, HOST_REPORT_SIZE);
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

void host_link_poll(void) {
    // Queued replies first, in order; one that can't go out yet waits for the next pass.
    uint8_t r[HOST_REPORT_SIZE];
    while (xQueuePeek(s_replies, r, 0) == pdTRUE) {
        if (!tud_hid_n_ready(s_instance) || !tud_hid_n_report(s_instance, 0, r, sizeof(r))) return;
        xQueueReceive(s_replies, r, 0);
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
}
