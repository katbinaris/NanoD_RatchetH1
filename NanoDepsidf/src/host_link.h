#pragma once

#include <stdbool.h>
#include <stdint.h>

// The desktop app's link over the vendor HID interface (host_proto.h has the wire format;
// the icon upload shares the interface, icon_store.h). usb_task.c drives it.

void host_link_init(uint8_t vendor_instance);

// TinyUSB task: one OUT report from the host. Builds the reply (if any) and queues it.
void host_link_handle_report(const uint8_t *report, uint16_t len);

// usb task, every pass: sends queued replies, then the live stream while the app asks for it.
void host_link_poll(void);

bool host_link_streaming(void);
void host_link_stop(void); // the host went away (unmounted): stop streaming, drop replies
