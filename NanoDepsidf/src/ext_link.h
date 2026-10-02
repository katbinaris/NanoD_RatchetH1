#pragma once

#include <stdbool.h>
#include <stdint.h>

// This fork's companion-protocol extensions (ext_proto.h), served next to host_link.c.

// TinyUSB task: one command in 0x20-0x2F -> at most one reply in `r` (64 bytes, zeroed).
bool ext_link_handle(const uint8_t *in, uint8_t *r);

// usb task, every pass: deferred work (a requested restart, once its ack has gone out).
void ext_link_poll(void);

// main.c, once, before the USB personality is chosen: true when the restart that led to this
// boot asked for a serial-only boot (EXT_REBOOT_SERIAL). Clears the request.
bool ext_take_serial_boot(void);
