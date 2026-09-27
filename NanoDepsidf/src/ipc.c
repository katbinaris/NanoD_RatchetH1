#include "ipc.h"
#include "esp_log.h"

static const char *TAG = "ipc";

QueueHandle_t g_hid_report_queue = NULL;
QueueHandle_t g_motor_cmd_queue = NULL;

void ipc_init(void) {
    // 32 deep -- a fast knob spin can generate many detent-crossing scroll events in a
    // short burst (measured up to ~200/s at 1000 RPM / 12 detents during Phase 2a
    // bring-up); usb_task.c drains this promptly (blocks on it with a short timeout, not
    // a slow poll), but a generous depth avoids dropping ticks from a burst that outruns
    // one drain cycle.
    g_hid_report_queue = xQueueCreate(32, sizeof(hid_report_msg_t));
    g_motor_cmd_queue = xQueueCreate(4, sizeof(motor_cmd_msg_t));
    if (g_hid_report_queue == NULL || g_motor_cmd_queue == NULL) {
        ESP_LOGE(TAG, "queue creation failed");
        abort();
    }
}
