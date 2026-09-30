#pragma once

#include <stdint.h>

// Quadra companion protocol (the desktop app, companion/): settings, live state and SYS INFO
// over the vendor HID interface, next to the icon upload (icon_store.h: commands 0x01-0x04,
// replies tagged 0xA0). The app mirrors this file in companion/src/proto.ts -- change both.
//
// Every report is 64 bytes, no report ID. [0] is the command (host -> device) or the reply tag
// (device -> host). Multi-byte fields little-endian, floats IEEE-754 single. Packed layouts
// below are byte offsets; unused bytes are 0.
//
// A deliberately small hand-rolled format for this first slice: a few fixed messages. The
// nanopb direction (DEVELOPMENT_PLAN.md Phase 9) stays for profile upload, where the
// structures get big enough for a schema to pay off.

#define HOST_PROTO_VERSION 1
#define HOST_REPORT_SIZE 64

// --- Host -> device ---
enum {
    HOST_CMD_HELLO = 0x10,        // -> HOST_TAG_HELLO
    HOST_CMD_GET_SETTINGS = 0x11, // -> HOST_TAG_SETTINGS
    HOST_CMD_SET = 0x12,          // [1]=HOST_SET_* [4..7]=value (i32, or f32 for the float ones)
                                  //   applied live (clamped like the menu) -> HOST_TAG_SETTINGS
    HOST_CMD_SAVE = 0x13,         // everything that differs from NVS is saved -> HOST_TAG_SETTINGS
    HOST_CMD_REVERT = 0x14,       // back to what NVS holds -> HOST_TAG_SETTINGS
    HOST_CMD_STREAM = 0x15,       // [1]=state rate in Hz (0 = stop, max 50). While on: HOST_TAG_STATE
                                  //   at that rate and HOST_TAG_SYS_A / _B twice a second.
    HOST_CMD_PROFILE = 0x16,      // [1]=index -> HOST_TAG_PROFILE
    HOST_CMD_PROFILE_ICON = 0x17, // [1]=index [2..3]=offset -> HOST_TAG_PROFILE_ICON (icon48)
    HOST_CMD_RESET_PEAKS = 0x18,  // SYS INFO peaks and counters start over (F1 there); no reply
};

// Setting ids for HOST_CMD_SET (and the order of HOST_TAG_SETTINGS' dirty bits).
enum {
    HOST_SET_DETENTS = 0,   // i32, HAPTIC_NUM_DETENTS_MIN..MAX
    HOST_SET_KP = 1,        // f32 (SNAP)
    HOST_SET_KD = 2,        // f32 (DAMP)
    HOST_SET_FEEL = 3,      // i32 haptic_type_t
    HOST_SET_AMP = 4,       // i32 percent
    HOST_SET_PITCH = 5,     // f32 multiplier
    HOST_SET_SOUND = 6,     // i32 audio_click_timbre_t
    HOST_SET_HID_TYPE = 7,  // i32 menu_hid_type_t
    HOST_SET_MIDI_CH = 8,   // i32 1..16
    HOST_SET_PROFILE = 9,   // i32 app profile index
    HOST_SET_BOOT = 10,     // i32 boot_usb_mode_t (SERIAL: the next boot has no HID, no app)
    HOST_SET_ROTATION = 11, // i32 quarter turns 0..3
    HOST_SET_HOST = 12,     // i32 menu_host_t
    HOST_SET_COUNT
};

// --- Device -> host ---
enum {
    HOST_TAG_HELLO = 0xB0,
    // [1]=HOST_PROTO_VERSION [2]=profile count [3]=boot mode running (0 HID, 1 SERIAL)
    // [4..35]=firmware version, NUL-terminated [36..51]=build date, NUL-terminated
    HOST_TAG_SETTINGS = 0xB1,
    // [1..2]=dirty bits (1 << HOST_SET_*: live differs from NVS)
    // [4..7]=detents i32 [8..11]=kp f32 [12..15]=kd f32 [16]=feel [17]=amp % [18..21]=pitch f32
    // [22]=sound [23]=hid type [24]=midi ch [25]=profile [26]=boot [27]=rotation [28]=host
    HOST_TAG_PROFILE = 0xB2,
    // [1]=index [2]=count [3]=has icon48 [4..15]=id [16..31]=name [32..63]=legend: 4 x 8 bytes
    // (all NUL-padded strings)
    HOST_TAG_PROFILE_ICON = 0xB3,
    // [1]=index [2..3]=offset [4]=len (<= 56) [8..63]=bytes of icon48 (48x48 RGB565 BE, 4608 B)
    HOST_TAG_STATE = 0xB5,
    // [1..2]=sequence [4..7]=knob angle i32, 1e-4 rad, continuous [8..11]=detent i32
    // [12]=buttons held (bit0 F1..bit3 F4) [13]=menu screen (menu_screen_id_t, 0 = closed)
    // [14]=screensaver on [15]=APP live slot [16..19]=clicks i32 [20..23]=end-stop hits i32
    HOST_TAG_SYS_A = 0xB6,
    // power + heat: [4..5]=motor mA [6..7]=led mA [8..9]=board mA [10..11]=total mA
    // [12..13]=total peak mA [14]=chip ok [16..19]=chip C f32 [20..23]=chip peak C f32
    // [24..25]=coil mA [26..27]=coil peak mA [28..31]=copper W f32
    // [32]=USB source (pd_source_t) [34..35]=USB mA [36..37]=USB mV
    HOST_TAG_SYS_B = 0xB7,
    // cpu + system: [4]=load core 0 % [5]=load core 1 % [6]=peak 0 [7]=peak 1
    // [8..11]=loop kHz f32 [12..15]=work avg us f32 [16..19]=work max us f32
    // [20..23]=jitter max us f32 [24..27]=missed u32 [28..31]=spikes/s f32
    // [32..35]=heap free u32 [36..39]=heap min u32 [40..43]=hid drops u32
    // [44..47]=audio gaps u32 [48..51]=uptime s u32 [52..55]=sensor CRC errors u32
    HOST_TAG_ERROR = 0xBF,
    // [1]=command [2]=HOST_ERR_*
};

enum {
    HOST_ERR_UNKNOWN_CMD = 1,
    HOST_ERR_BAD_PARAM = 2,
};

#define HOST_ICON_CHUNK 56
