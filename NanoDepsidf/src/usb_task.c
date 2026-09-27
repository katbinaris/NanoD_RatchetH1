#include "usb_task.h"
#include "tasks_common.h"
#include "ipc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#include "class/hid/hid_device.h"

static const char *TAG = "usb";

// --- Phase 3: USB HID (keyboard / mouse / gamepad) + CDC-ACM ---
// First working slice: a composite TinyUSB device -- one CDC-ACM serial port (2
// interfaces) plus one HID interface carrying three report IDs (keyboard, mouse,
// gamepad), following the exact pattern of the ESP-IDF reference examples this is adapted
// from (examples/peripherals/usb/device/tusb_hid for the HID half, the upstream TinyUSB
// cdc_msc example for the CDC-composite pattern -- not a legacy_fw port, neither existed
// there). Enumeration and report delivery both confirmed on hardware (HID recognized by
// host, a keyboard self-test 'a' tap was visibly typed) -- the self-test itself has since
// been removed now that it's served its purpose.
//
// First real (non-self-test) mapping: knob rotation -> mouse scroll wheel, one detent
// crossing = one step (`HID_EVENT_MOUSE_WHEEL`, produced in `control_task.c` at the same
// detent edge that drives the haptic click, consumed here). This is the first concrete
// answer to Phase 3's "how much of the mapping engine survives" open decision -- not the
// whole answer; keys and the rest of the mapping surface are still open.
//
// CDC-ACM is not optional here, even though Phase 3's own scope is just HID: this board's
// upload flow (`use_1200bps_touch` in boards/nanofoc_d.json) depends on the *running*
// firmware exposing a USB-CDC serial port so PlatformIO/esptool can open it and toggle
// 1200 baud to trigger a reset into the bootloader. A HID-only descriptor has no such
// port, which would silently break `pio run -t upload` -- falling back to the physical
// BOOT switch every time, which is reserved for brick recovery, not routine reflashing.
// This also fixes the secondary-console conflict flagged in Phase 7's plan entry (the
// built-in USB-Serial-JTAG console can't coexist with TinyUSB on the same physical OTG
// PHY) by giving the running firmware its own CDC console instead, via
// `tinyusb_console_init()` -- redirects stdio (all ESP_LOGx output included) onto this
// port once it's up. Endpoint budget confirmed directly from TinyUSB's dwc2_esp32.h port
// source (ESP32-S3: 7 total endpoint numbers, 5 usable IN) rather than assumed: CDC needs
// 2 IN + 1 OUT, HID's 3 report IDs share a single IN endpoint -- 3 IN + 1 OUT total,
// comfortably inside budget, so nothing had to be dropped from the HID side to fit CDC in.
//
// Key input still isn't wired up: the actual key-input driver (GPIO read/debounce for
// BTN_A-D, owned by Core 0 per the architecture log in DEVELOPMENT_PLAN.md) doesn't exist
// yet, and how much of the rest of the mapping engine survives is still open (see
// DEVELOPMENT_PLAN.md Phase 3's "Open decisions" list).

enum {
    ITF_NUM_CDC = 0,      // CDC control interface (CDC data is ITF_NUM_CDC + 1, implicit
                          // in TUD_CDC_DESCRIPTOR's own Interface Association Descriptor)
    ITF_NUM_CDC_DATA = 1,
    ITF_NUM_HID = 2,
    ITF_NUM_TOTAL = 3,
};

enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE = 2,
    REPORT_ID_GAMEPAD = 3,
};

#define EPNUM_CDC_NOTIF 0x81 // EP1 IN
#define EPNUM_CDC_OUT   0x02 // EP2 OUT
#define EPNUM_CDC_IN    0x82 // EP2 IN
#define EPNUM_HID_IN    0x83 // EP3 IN

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(REPORT_ID_GAMEPAD)),
};

static const char *s_usb_string_descriptor[6] = {
    (char[]){0x09, 0x04}, // 0: supported language -- English (0x0409)
    "Binaris Circuitry",  // 1: Manufacturer
    "Nano D++",           // 2: Product
    "NANOD-DEV",          // 3: Serial -- placeholder; a real per-device ID (e.g. from
                           //    efuse MAC) is follow-on work, not needed for this slice
    "NanoD Console",       // 4: CDC interface name
    "NanoD HID",           // 5: HID interface name
};

// device descriptor deliberately left NULL in tinyusb_config_t below: esp_tinyusb's own
// default device descriptor already sets bDeviceClass/SubClass/Protocol to
// MISC/COMMON/IAD whenever CFG_TUD_CDC > 0 (confirmed directly in its
// usb_descriptors.c) -- exactly what a CDC-composite device needs, so there's no need to
// hand-roll a custom device descriptor just to get those three fields right.
static const uint8_t s_usb_configuration_descriptor[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, EP notification address & size, EP data (out, in) & size
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 5, false, sizeof(s_hid_report_descriptor), EPNUM_HID_IN, 16, 10),
};

// --- Required TinyUSB HID callbacks (no weak default -- must be defined) ---

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance; // single HID interface -- nothing to select on
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    ESP_LOGI(TAG, "USB suspended");
}

void tud_resume_cb(void) {
    ESP_LOGI(TAG, "USB resumed");
}

static void usb_task_fn(void *arg) {
    ESP_LOGI(TAG, "usb task started on core %d, prio %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = NULL; // see comment above s_usb_configuration_descriptor
    tusb_cfg.descriptor.full_speed_config = s_usb_configuration_descriptor;
    tusb_cfg.descriptor.string = s_usb_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(s_usb_string_descriptor) / sizeof(s_usb_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_usb_configuration_descriptor; // S3 OTG is
                                                                             // FS-only, kept
                                                                             // for portability
#endif
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    const tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
    // Redirects stdio (all ESP_LOGx output included) onto this CDC port -- restores a
    // usable serial console over the same USB-C cable now that the built-in
    // USB-Serial-JTAG console can no longer share the OTG PHY with TinyUSB (see header
    // comment). Primary UART0 console is untouched by this and keeps working independently
    // via its own separate pins.
    ESP_ERROR_CHECK(tinyusb_console_init(TINYUSB_CDC_ACM_0));

    ESP_LOGI(TAG, "USB composite device installed (CDC console + HID keyboard+mouse+gamepad)");

    // Blocks on the queue itself (not a fixed-interval poll) so a scroll event reaches
    // the host with minimal added latency -- the timeout just bounds how long this task
    // can sit idle, it isn't a polling period.
    hid_report_msg_t msg;
    while (1) {
        if (xQueueReceive(g_hid_report_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (msg.type) {
                case HID_EVENT_MOUSE_WHEEL:
                    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, 0, 0, msg.wheel_delta, 0);
                    break;
            }
        }
    }
}

void usb_task_start(void) {
    xTaskCreatePinnedToCore(usb_task_fn, "usb", 4096, NULL, PRIO_USB, NULL, CORE_IO);
}
