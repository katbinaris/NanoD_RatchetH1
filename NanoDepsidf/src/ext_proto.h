#pragma once

// Extensions to the companion protocol (host_proto.h) for this fork. Same framing: 64-byte
// reports on the vendor HID interface, no report ID, little-endian. Kept in their own command
// range (0x20-0x2F, replies and events 0xC0-0xCF) so upstream can grow 0x10-0x1F freely.
// Host side: tools/quadra.py.

#define EXT_PROTO_VERSION 1

// --- Host -> device ---
enum {
    EXT_CMD_HELLO = 0x20,  // -> EXT_TAG_HELLO
    EXT_CMD_REBOOT = 0x21, // [1]=EXT_REBOOT_* -> EXT_TAG_ACK, then the device restarts
};

enum {
    EXT_REBOOT_NORMAL = 0,
    // The next boot only comes up as the chip's USB-Serial-JTAG port (no HID), like holding
    // F3+F4 at power-on: esptool can then reset it into the ROM loader and flash it, and its
    // reset after flashing boots normally again. One boot only -- nothing is stored.
    EXT_REBOOT_SERIAL = 1,
};

// --- Device -> host ---
enum {
    EXT_TAG_HELLO = 0xC0, // [1]=EXT_PROTO_VERSION
    EXT_TAG_ACK = 0xC1,   // [1]=command [2]=EXT_ST_*
};

enum {
    EXT_ST_OK = 0,
    EXT_ST_BAD_PARAM = 1,
    EXT_ST_UNKNOWN = 2,
};
