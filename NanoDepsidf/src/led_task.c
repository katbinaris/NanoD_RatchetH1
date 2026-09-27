#include "led_task.h"
#include "tasks_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "led";

// Stub -- real RMT-based led_strip driver is Phase 5.
static void led_task_fn(void *arg) {
    ESP_LOGI(TAG, "led task started on core %d, prio %d (stub, led_strip lands in Phase 5)",
             xPortGetCoreID(), uxTaskPriorityGet(NULL));
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void led_task_start(void) {
    xTaskCreatePinnedToCore(led_task_fn, "led", 4096, NULL, PRIO_LED, NULL, CORE_IO);
}
