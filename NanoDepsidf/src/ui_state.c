#include "ui_state.h"
#include <stdatomic.h>

static _Atomic int32_t s_detent = 0;
static _Atomic uint8_t s_buttons = 0;
static _Atomic bool s_usb_serial_active = false;
static _Atomic bool s_screensaver = false;

void ui_state_init(void) {
    atomic_store_explicit(&s_detent, 0, memory_order_relaxed);
    atomic_store_explicit(&s_buttons, 0, memory_order_relaxed);
    atomic_store_explicit(&s_usb_serial_active, false, memory_order_relaxed);
    atomic_store_explicit(&s_screensaver, false, memory_order_relaxed);
}

void ui_state_set_detent(int32_t wrapped_index) {
    atomic_store_explicit(&s_detent, wrapped_index, memory_order_relaxed);
}

int32_t ui_state_get_detent(void) {
    return atomic_load_explicit(&s_detent, memory_order_relaxed);
}

void ui_state_set_buttons(uint8_t held_mask) {
    atomic_store_explicit(&s_buttons, held_mask, memory_order_relaxed);
}

uint8_t ui_state_get_buttons(void) {
    return atomic_load_explicit(&s_buttons, memory_order_relaxed);
}

void ui_state_set_usb_serial_active(bool serial) {
    atomic_store_explicit(&s_usb_serial_active, serial, memory_order_relaxed);
}

bool ui_state_get_usb_serial_active(void) {
    return atomic_load_explicit(&s_usb_serial_active, memory_order_relaxed);
}

void ui_state_set_screensaver(bool active) {
    atomic_store_explicit(&s_screensaver, active, memory_order_relaxed);
}

bool ui_state_get_screensaver(void) {
    return atomic_load_explicit(&s_screensaver, memory_order_relaxed);
}
