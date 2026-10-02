#include "ext_link.h"
#include "ext_proto.h"
#include "host_link.h"
#include "host_proto.h"
#include "menu.h"
#include "notify.h"
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
