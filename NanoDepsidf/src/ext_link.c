#include "ext_link.h"
#include "ext_proto.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

static const char *TAG = "ext";

// The one-boot serial request. RTC_NOINIT memory survives esp_restart() but not a power cycle
// or the EN button, and the reset reason is checked too, so stale contents can't trigger it.
#define SERIAL_BOOT_MAGIC 0x5E71A1B0u
static RTC_NOINIT_ATTR uint32_t s_serial_boot;

// A requested restart waits this long so its ack (queued for the usb task) reaches the host.
#define REBOOT_DELAY_TICKS 25 // x 10ms
static _Atomic bool s_reboot_pending = false;
static TickType_t s_reboot_at;

bool ext_take_serial_boot(void) {
    bool requested = s_serial_boot == SERIAL_BOOT_MAGIC && esp_reset_reason() == ESP_RST_SW;
    s_serial_boot = 0;
    return requested;
}

bool ext_link_handle(const uint8_t *in, uint8_t *r) {
    switch (in[0]) {
        case EXT_CMD_HELLO:
            r[0] = EXT_TAG_HELLO;
            r[1] = EXT_PROTO_VERSION;
            return true;
        case EXT_CMD_REBOOT:
            r[0] = EXT_TAG_ACK;
            r[1] = in[0];
            if (in[1] > EXT_REBOOT_SERIAL) {
                r[2] = EXT_ST_BAD_PARAM;
                return true;
            }
            s_serial_boot = in[1] == EXT_REBOOT_SERIAL ? SERIAL_BOOT_MAGIC : 0;
            s_reboot_at = xTaskGetTickCount() + REBOOT_DELAY_TICKS;
            atomic_store(&s_reboot_pending, true);
            r[2] = EXT_ST_OK;
            return true;
        default:
            r[0] = EXT_TAG_ACK;
            r[1] = in[0];
            r[2] = EXT_ST_UNKNOWN;
            return true;
    }
}

void ext_link_poll(void) {
    if (atomic_load(&s_reboot_pending) && (int32_t)(xTaskGetTickCount() - s_reboot_at) >= 0) {
        ESP_LOGW(TAG, "restart requested by the host%s", s_serial_boot ? " (serial boot)" : "");
        esp_restart();
    }
}
