#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "board_pins.h"
#include "ipc.h"
#include "audio_trigger.h"
#include "ui_state.h"
#include "menu.h"
#include "control_task.h"
#include "usb_task.h"
#include "i2s_task.h"
#include "display_task.h"
#include "led_task.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "nanod";

// Phase 3 USB-mode select, checked once here before any task starts (not folded into
// control_task.c's own BTN_A-prefixed arm-window logic -- this is a different, unrelated
// decision: which USB personality to bring up, not which motor/haptic diagnostic to run).
//
// Our composite TinyUSB device (CDC console + HID keyboard/mouse/gamepad, usb_task.c)
// claims the chip's one physical USB-OTG PHY away from the built-in USB-Serial-JTAG
// peripheral, which is what transparently handled console + `pio run -t upload`'s
// 1200bps-touch reset-to-bootloader trick throughout Phases 0-2 with zero app code at
// all (CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG, sdkconfig.esp32-s3-devkitm-1) --
// TinyUSB CDC-ACM doesn't get that reset trick for free (there's no
// tud_cdc_line_coding_cb() implementing it), and porting the register-level PHY-mux
// dance ESP32-Arduino's usb_persist_restart() uses to do it manually is real, hard-to-
// verify-without-hardware chip-specific complexity -- see the DEVELOPMENT_PLAN.md Phase 3
// entry for that history. Sidestepping it entirely: hold BTN_C+BTN_D at boot to skip
// installing TinyUSB altogether, leaving the PHY on its default USB-Serial-JTAG routing
// (confirmed default from every prior phase's bring-up) -- normal flashing/console just
// works, exactly as it always did, at the cost of HID being unavailable for that boot.
// Release both (normal boot) to get the composite HID+CDC device instead.
#define USB_MODE_SELECT_POLL_ITERS 40 // 40 x 50ms = 2s -- long enough to reliably catch a
                                      // held combo without needing exact boot-instant
                                      // timing (same lesson control_task.c's own BTN_A
                                      // arm-window comment documents)

static bool usb_serial_mode_requested(void) {
    gpio_config_t btn_cfg = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_BTN_C) | (1ULL << PIN_BTN_D),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_cfg);

    for (int i = 0; i < USB_MODE_SELECT_POLL_ITERS; i++) {
        bool c_pressed = (gpio_get_level(PIN_BTN_C) == 0); // active-low, pull-up
        bool d_pressed = (gpio_get_level(PIN_BTN_D) == 0);
        if (c_pressed && d_pressed) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

static const char *reset_reason_str(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_SW:        return "SW (esp_restart)";
        case ESP_RST_PANIC:     return "PANIC (crash)";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "OTHER_WDT";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_USB:       return "USB";
        default:                return "OTHER/UNKNOWN";
    }
}

void app_main(void) {
    // Logged unconditionally at every boot -- became important during Phase 2a motor
    // bring-up, where the board reset repeatedly under motor load and we needed hard
    // evidence of *why* (brownout vs. crash vs. watchdog) instead of guessing.
    ESP_LOGW(TAG, "Reset reason: %s", reset_reason_str(esp_reset_reason()));

    ESP_LOGI(TAG, "Nano_D++ ESP-IDF bring-up, LCD_CS=%d, MAG_CS=%d", PIN_LCD_CS, PIN_MAG_CS);

    // Standard erase-and-retry pattern: the NVS partition can be reported as needing an
    // erase after a truncated write or a partition layout change (e.g. from earlier
    // bring-up when the partition table itself was being iterated on).
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (%s) -- erasing and retrying", esp_err_to_name(nvs_err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    ipc_init();
    audio_trigger_init();
    ui_state_init();
    menu_init();

    // See usb_serial_mode_requested()'s comment above -- checked before any task starts.
    bool usb_serial_mode = usb_serial_mode_requested();
    if (usb_serial_mode) {
        ESP_LOGW(TAG, "BTN_C+BTN_D held at boot -- USB serial mode: TinyUSB (HID) will NOT "
                       "be installed this boot. Native USB-Serial-JTAG console/flashing "
                       "stays on its default routing -- flash normally now.");
    }

    // Core 0: control loop, kept exclusive per DEVELOPMENT_PLAN.md
    control_task_start();

    // Core 1: everything DMA-offloaded/tolerant
    if (!usb_serial_mode) {
        usb_task_start();
    } else {
        ESP_LOGI(TAG, "usb task not started (USB serial mode active this boot)");
    }
    // Diagnostic disable (DEVELOPMENT_PLAN.md Phase 8) confirmed I2S was NOT the cause of
    // the laggy roller animation -- re-enabled.
    i2s_task_start();
    display_task_start();
    led_task_start();
}
