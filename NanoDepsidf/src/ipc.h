#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Real HID report shape (Phase 3) -- carries just enough for usb_task.c to build the
// actual TinyUSB report call, not raw report bytes, so producers (control_task.c) don't
// need to know TinyUSB's API at all. Only the knob->scroll-wheel mapping exists so far;
// extend this enum/struct as more of the mapping engine (Phase 3's still-pending "Open
// decisions" item) gets designed -- keys, mouse move, gamepad, etc.
// APP mode's drags and pivot don't go through this queue: they're state, polled by
// usb_task.c from app_mode.h, so a dropped report can't leave a button stuck down.
typedef enum {
    HID_EVENT_MOUSE_WHEEL,
} hid_event_type_t;

typedef struct {
    hid_event_type_t type;
    union {
        int8_t wheel_delta; // HID_EVENT_MOUSE_WHEEL: signed wheel steps, +/- per HID_WHEEL_SIGN
    };
} hid_report_msg_t;

// Motor cmd message shape below is still the Phase 1 placeholder -- unrelated to the above,
// refined separately when Phase 8 gives it a real producer.

typedef struct {
    uint32_t cmd;
    int32_t value;
} motor_cmd_msg_t;

// Control core (0) -> USB task (1): finished HID reports ready to send.
extern QueueHandle_t g_hid_report_queue;

// IO core (1) -> control core (0): commands such as calibration trigger, profile change.
// Defined now, not yet driven by a real producer -- that lands with Phase 8.
extern QueueHandle_t g_motor_cmd_queue;

void ipc_init(void);
