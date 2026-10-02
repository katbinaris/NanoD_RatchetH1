#pragma once

// Extensions to the companion protocol (host_proto.h) for this fork. Same framing: 64-byte
// reports on the vendor HID interface, no report ID, little-endian. Kept in their own command
// range (0x20-0x2F, replies and events 0xC0-0xCF) so upstream can grow 0x10-0x1F freely.
// Host side: tools/quadra.py, tools/agents/.

#define EXT_PROTO_VERSION 2

// --- Host -> device ---
enum {
    EXT_CMD_HELLO = 0x20,  // -> EXT_TAG_HELLO
    EXT_CMD_REBOOT = 0x21, // [1]=EXT_REBOOT_* -> EXT_TAG_ACK, then the device restarts
    EXT_CMD_TEXT = 0x22,   // [1]=0 (the idle text) [2..17]=text, NUL-padded, <= USER_TEXT_MAX
                           //   (user_prefs.h); "" = the stock wordmark. Stored. -> EXT_TAG_ACK
    EXT_CMD_LIGHTS = 0x23, // [1]=EXT_LIGHTS_* flags [2]=src [3]=fx [4..5]=hue [6]=sat [7]=speed
                           //   [8..9]=level (user_prefs.h lights_t); 0xFF / 0xFFFF = keep that
                           //   field. Live at once, like turning the knob. -> EXT_TAG_PREFS
    EXT_CMD_PREFS = 0x24,  // -> EXT_TAG_PREFS
    EXT_CMD_NOTIFY = 0x25, // [1]=EXT_NOTIFY_* [2..3]=id; POST: [4]=source [5]=kind (notify.h)
                           //   [6..21]=title [22..63]=body, NUL-padded. No reply.
};

enum {
    EXT_REBOOT_NORMAL = 0,
    // The next boot only comes up as the chip's USB-Serial-JTAG port (no HID), like holding
    // F3+F4 at power-on: esptool can then reset it into the ROM loader and flash it, and its
    // reset after flashing boots normally again. One boot only -- nothing is stored.
    EXT_REBOOT_SERIAL = 1,
};

#define EXT_LIGHTS_SAVE 0x01 // also store them (otherwise live only, shown as unsaved)

enum { EXT_NOTIFY_POST = 1, EXT_NOTIFY_CLEAR = 2, EXT_NOTIFY_CLEAR_ALL = 3 };

// --- Device -> host ---
enum {
    EXT_TAG_HELLO = 0xC0,  // [1]=EXT_PROTO_VERSION
    EXT_TAG_ACK = 0xC1,    // [1]=command [2]=EXT_ST_*
    EXT_TAG_PREFS = 0xC2,  // [1]=src [2]=fx [3..4]=hue [5]=sat [6]=speed [7..8]=level
                           // [9]=1: the lights differ from what's saved [16..31]=idle text
    EXT_TAG_NOTIFY = 0xC3, // unsolicited: [1]=notify_decision_t [2..3]=id
};

enum {
    EXT_ST_OK = 0,
    EXT_ST_BAD_PARAM = 1,
    EXT_ST_UNKNOWN = 2,
    EXT_ST_STORAGE = 3, // the NVS write failed
};
