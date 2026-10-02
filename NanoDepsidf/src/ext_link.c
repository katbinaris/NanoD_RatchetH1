#include "ext_link.h"
#include "ext_proto.h"
#include "host_link.h"
#include "host_proto.h"
#include "menu.h"
#include "notify.h"
#include "media.h"
#include "agent_board.h"
#include "net.h"
#include "tasks_common.h"
#include "user_prefs.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "ext";

// The one-boot serial request. RTC_NOINIT memory survives esp_restart() but not a power cycle
// or the EN button, and the reset reason is checked too, so stale contents can't trigger it.
#define SERIAL_BOOT_MAGIC 0x5E71A1B0u
static RTC_NOINIT_ATTR uint32_t s_serial_boot;

// A requested restart: the control task does it (motor off first, like RECALIBRATE) once the
// ack has had time to reach the host; the usb task steps in if the control loop isn't running.
#define REBOOT_DELAY_TICKS 25     // x 10ms
#define REBOOT_FALLBACK_TICKS 150 // x 10ms
static _Atomic bool s_reboot_pending = false;
static TickType_t s_reboot_at;

// Requests that write NVS run in the usb task (ext_link_poll), not the TinyUSB task they
// arrive in, the way host_link.c hands a profile save over.
static char s_text_req[USER_TEXT_MAX + 1];
static _Atomic bool s_text_pending = false;
static lights_t s_lights_req;
static bool s_lights_save;
static _Atomic bool s_lights_pending = false;
static _Atomic bool s_cover_end_pending = false;
// WiFi setup, staged until APPLY (EXT_CMD_NET). The password is wiped once it's stored.
static char s_net_ssid[NET_SSID_MAX + 1], s_net_pass[64];
static bool s_net_have_ssid, s_net_have_pass, s_net_on;
static _Atomic bool s_net_apply_pending = false;

bool ext_take_serial_boot(void) {
    bool requested = s_serial_boot == SERIAL_BOOT_MAGIC && esp_reset_reason() == ESP_RST_SW;
    s_serial_boot = 0;
    return requested;
}

bool CONTROL_HOT ext_restart_due(void) {
    return atomic_load_explicit(&s_reboot_pending, memory_order_relaxed)
        && (int32_t)(xTaskGetTickCount() - s_reboot_at) >= 0;
}

static void put_u16(uint8_t *b, uint16_t v) { memcpy(b, &v, 2); }

static void build_prefs(uint8_t *r) {
    lights_t l;
    lights_get(&l);
    r[0] = EXT_TAG_PREFS;
    r[1] = (uint8_t)l.src;
    r[2] = (uint8_t)l.fx;
    put_u16(r + 3, (uint16_t)l.hue);
    r[5] = (uint8_t)l.sat;
    r[6] = (uint8_t)l.speed;
    put_u16(r + 7, (uint16_t)l.level);
    r[9] = menu_lights_dirty();
    user_text_get((char *)r + 16, USER_TEXT_MAX + 1);
}

static void ack(uint8_t *r, uint8_t cmd, uint8_t status) {
    r[0] = EXT_TAG_ACK;
    r[1] = cmd;
    r[2] = status;
}

static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static void build_net(uint8_t *r) {
    net_status_t st;
    net_status(&st);
    r[0] = EXT_TAG_NET;
    r[1] = (uint8_t)st.state;
    r[2] = (uint8_t)st.rssi;
    memcpy(r + 3, &st.ip, 4); // network order: a.b.c.d
    r[7] = st.time_set;
    r[8] = st.enabled;
    memcpy(r + 9, st.ssid, strnlen(st.ssid, NET_SSID_MAX));
    memcpy(r + 41, st.host, strnlen(st.host, NET_HOST_MAX));
}

bool ext_link_handle(const uint8_t *in, uint8_t *r) {
    switch (in[0]) {
        case EXT_CMD_HELLO:
            r[0] = EXT_TAG_HELLO;
            r[1] = EXT_PROTO_VERSION;
            return true;
        case EXT_CMD_REBOOT:
            if (in[1] > EXT_REBOOT_SERIAL) {
                ack(r, in[0], EXT_ST_BAD_PARAM);
                return true;
            }
            s_serial_boot = in[1] == EXT_REBOOT_SERIAL ? SERIAL_BOOT_MAGIC : 0;
            s_reboot_at = xTaskGetTickCount() + REBOOT_DELAY_TICKS;
            atomic_store(&s_reboot_pending, true);
            ack(r, in[0], EXT_ST_OK);
            return true;
        case EXT_CMD_TEXT:
            if (in[1] != 0 || atomic_load(&s_text_pending)) {
                ack(r, in[0], EXT_ST_BAD_PARAM);
                return true;
            }
            memcpy(s_text_req, in + 2, USER_TEXT_MAX);
            s_text_req[USER_TEXT_MAX] = '\0';
            atomic_store(&s_text_pending, true); // acked from ext_link_poll once stored
            return false;
        case EXT_CMD_LIGHTS: {
            if (atomic_load(&s_lights_pending)) {
                ack(r, in[0], EXT_ST_BAD_PARAM);
                return true;
            }
            lights_t l;
            lights_get(&l);
            if (in[2] != 0xFF) l.src = in[2];
            if (in[3] != 0xFF) l.fx = in[3];
            if (rd_u16(in + 4) != 0xFFFF) l.hue = rd_u16(in + 4);
            if (in[6] != 0xFF) l.sat = in[6];
            if (in[7] != 0xFF) l.speed = in[7];
            if (rd_u16(in + 8) != 0xFFFF) l.level = rd_u16(in + 8);
            s_lights_req = l;
            s_lights_save = in[1] & EXT_LIGHTS_SAVE;
            atomic_store(&s_lights_pending, true);
            return false;
        }
        case EXT_CMD_PREFS:
            build_prefs(r);
            return true;
        case EXT_CMD_COVER:
            if (in[1] == EXT_COVER_BEGIN) {
                uint32_t len, crc;
                memcpy(&len, in + 4, 4);
                memcpy(&crc, in + 8, 4);
                ack(r, in[0], media_cover_begin(len, crc) ? EXT_ST_OK : EXT_ST_BAD_PARAM);
                return true;
            }
            if (in[1] == EXT_COVER_DATA) {
                uint32_t off = in[2] | in[3] << 8 | (uint32_t)in[4] << 16;
                media_cover_data(off, in + 6, in[5] <= EXT_COVER_CHUNK ? in[5] : 0); // a failure shows at END
                return false;
            }
            if (in[1] == EXT_COVER_END) {
                atomic_store(&s_cover_end_pending, true); // the CRC runs in the usb task
                return false;
            }
            ack(r, in[0], EXT_ST_BAD_PARAM);
            return true;
        case EXT_CMD_TRACK:
            if (in[1] & EXT_TRACK_NONE) {
                media_clear();
            } else {
                media_track_t t = {.playing = in[1] & EXT_TRACK_PLAYING, .volume = in[2] <= 100 ? (int8_t)in[2] : -1};
                for (int i = 0; i < 3; i++) t.palette[i] = (uint32_t)in[3 + i * 3] << 16 | (uint32_t)in[4 + i * 3] << 8 | in[5 + i * 3];
                memcpy(t.title, in + 12, MEDIA_TEXT_MAX);
                memcpy(t.artist, in + 36, MEDIA_TEXT_MAX);
                media_set_track(&t);
            }
            return false;
        case EXT_CMD_AGENTS: {
            agent_row_t rows[AGENT_BOARD_MAX] = {0};
            int n = in[1] <= AGENT_BOARD_MAX ? in[1] : AGENT_BOARD_MAX;
            for (int i = 0; i < n; i++) {
                const uint8_t *p = in + 2 + i * 14;
                rows[i].source = p[0] < NOTIFY_SRC_COUNT ? p[0] : NOTIFY_SRC_OTHER;
                rows[i].state = p[1] < AGENT_STATE_COUNT ? p[1] : AGENT_IDLE;
                memcpy(rows[i].name, p + 2, AGENT_NAME_MAX);
            }
            agent_board_set(rows, n);
            return false;
        }
        case EXT_CMD_NOTIFY: {
            uint16_t id = rd_u16(in + 2);
            uint8_t kind = in[5] & 0x7F;
            if (in[1] == EXT_NOTIFY_POST && in[4] < NOTIFY_SRC_COUNT && kind < NOTIFY_KIND_COUNT) {
                notify_item_t it = {
                    .id = id, .source = in[4], .kind = kind,
                    .flags = (in[5] & EXT_NOTIFY_NUDGE) ? NOTIFY_FLAG_NUDGE : 0,
                    .color = (uint32_t)in[61] << 16 | (uint32_t)in[62] << 8 | in[63],
                };
                memcpy(it.title, in + 6, NOTIFY_TITLE_MAX);
                memcpy(it.body, in + 22, NOTIFY_BODY_MAX);
                notify_post(&it);
            } else if (in[1] == EXT_NOTIFY_CLEAR) {
                notify_clear(id);
            } else if (in[1] == EXT_NOTIFY_CLEAR_ALL) {
                notify_clear_all();
            }
            return false;
        }
        case EXT_CMD_NET:
            if (atomic_load(&s_net_apply_pending) && in[1] != EXT_NET_STATUS) {
                ack(r, in[0], EXT_ST_BAD_PARAM);
                return true;
            }
            switch (in[1]) {
                case EXT_NET_SSID:
                    memcpy(s_net_ssid, in + 2, NET_SSID_MAX);
                    s_net_ssid[NET_SSID_MAX] = '\0';
                    s_net_have_ssid = true;
                    return false;
                case EXT_NET_PASS_A:
                    memset(s_net_pass, 0, sizeof(s_net_pass));
                    memcpy(s_net_pass, in + 2, 32);
                    s_net_have_pass = true;
                    return false;
                case EXT_NET_PASS_B:
                    memcpy(s_net_pass + 32, in + 2, 31); // 63 characters at most, NUL-ended
                    return false;
                case EXT_NET_APPLY:
                    s_net_on = in[2] == 1;
                    atomic_store(&s_net_apply_pending, true); // stored in ext_link_poll
                    return false;
                case EXT_NET_STATUS:
                    build_net(r);
                    return true;
                default:
                    ack(r, in[0], EXT_ST_BAD_PARAM);
                    return true;
            }
        default:
            ack(r, in[0], EXT_ST_UNKNOWN);
            return true;
    }
}

void ext_link_poll(void) {
    uint8_t r[HOST_REPORT_SIZE];
    if (atomic_load(&s_text_pending)) {
        bool ok = user_text_set(s_text_req);
        memset(r, 0, sizeof(r));
        ack(r, EXT_CMD_TEXT, ok ? EXT_ST_OK : EXT_ST_STORAGE);
        host_link_queue(r);
        atomic_store(&s_text_pending, false);
    }
    if (atomic_load(&s_lights_pending)) {
        lights_set(&s_lights_req);
        if (s_lights_save) menu_remote_save_lights();
        memset(r, 0, sizeof(r));
        build_prefs(r);
        host_link_queue(r);
        atomic_store(&s_lights_pending, false);
    }
    if (atomic_load(&s_cover_end_pending)) {
        bool ok = media_cover_end();
        memset(r, 0, sizeof(r));
        ack(r, EXT_CMD_COVER, ok ? EXT_ST_OK : EXT_ST_BAD_PARAM);
        host_link_queue(r);
        atomic_store(&s_cover_end_pending, false);
    }
    if (atomic_load(&s_net_apply_pending)) {
        bool ok = net_configure(s_net_have_ssid ? s_net_ssid : NULL, s_net_have_pass ? s_net_pass : NULL, s_net_on);
        memset(s_net_pass, 0, sizeof(s_net_pass));
        s_net_have_ssid = s_net_have_pass = false;
        memset(r, 0, sizeof(r));
        if (ok) build_net(r);
        else ack(r, EXT_CMD_NET, EXT_ST_STORAGE);
        host_link_queue(r);
        atomic_store(&s_net_apply_pending, false);
    }
    uint16_t id;
    uint8_t decision;
    while (notify_take_event(&id, &decision)) {
        memset(r, 0, sizeof(r));
        r[0] = EXT_TAG_NOTIFY;
        r[1] = decision;
        put_u16(r + 2, id);
        host_link_queue(r);
    }
    if (atomic_load(&s_reboot_pending) && (int32_t)(xTaskGetTickCount() - s_reboot_at) >= REBOOT_FALLBACK_TICKS) {
        ESP_LOGW(TAG, "restart requested by the host -- the control loop didn't take it, restarting from here");
        esp_restart();
    }
}
