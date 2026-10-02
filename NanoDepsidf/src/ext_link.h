#pragma once

#include <stdbool.h>
#include <stdint.h>

// This fork's companion-protocol extensions (ext_proto.h), served next to host_link.c.

// TinyUSB task: one command in 0x20-0x2F -> at most one reply in `r` (64 bytes, zeroed).
bool ext_link_handle(const uint8_t *in, uint8_t *r);

// usb task, every pass: deferred work (NVS writes, decision events to the host, the restart
// fallback).
void ext_link_poll(void);

// Control task, every tick (CONTROL_HOT): true once a host-requested restart is due -- the
// loop switches the motor off and restarts, as for RECALIBRATE.
bool ext_restart_due(void);

// Control task, every tick (CONTROL_HOT): the companion's hands on the knob (EXT_CMD_INPUT) --
// the keys it holds down (UI_BTN_*), and the next step of a turn it asked for (-1 / 0 / +1).
uint8_t ext_virtual_keys(void);
int8_t ext_take_virtual_turn(void);

// The host went away: no more screen stream, and its keys let go.
void ext_link_stop(void);

// main.c, once, before the USB personality is chosen: true when the restart that led to this
// boot asked for a serial-only boot (EXT_REBOOT_SERIAL). Clears the request.
bool ext_take_serial_boot(void);
