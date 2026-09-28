#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Icon upload over the vendor HID interface (usb_task.c, HID instance 1) -> SRAM -> Main
// Screen (display_task.cpp). RAM only for now: the icon is gone after a reboot.
//
// Wire protocol: every report is exactly ICON_HID_REPORT_SIZE bytes (no report ID on the
// dedicated vendor interface), little-endian multi-byte fields.
//
// Host -> device (OUT):
//   ICON_CMD_BEGIN  [0]=cmd [1]=width [2]=height [3]=format [4..5]=total_len [6..9]=crc32
//   ICON_CMD_DATA   [0]=cmd [1..2]=offset [3]=len (<= ICON_DATA_MAX) [4..]=payload
//   ICON_CMD_END    [0]=cmd                    -> verify length + CRC, then commit
//   ICON_CMD_CLEAR  [0]=cmd                    -> remove the current icon
// DATA chunks must arrive in order (offset == bytes received so far); anything else aborts
// the transfer with ICON_ST_BAD_OFFSET so a dropped packet can never commit a torn image.
//
// Device -> host (IN), sent for BEGIN / END / CLEAR, and for any DATA that fails:
//   [0]=ICON_REPLY_TAG [1]=cmd being answered [2]=status [3..4]=bytes received so far
//
// CRC is standard CRC-32 (IEEE 802.3, reflected, init/xorout 0xFFFFFFFF -- same as
// Python's zlib.crc32) over the pixel bytes.

#define ICON_HID_REPORT_SIZE 64
#define ICON_DATA_HEADER 4
#define ICON_DATA_MAX (ICON_HID_REPORT_SIZE - ICON_DATA_HEADER) // 60 bytes per chunk

#define ICON_WIDTH 48
#define ICON_HEIGHT 48
#define ICON_FORMAT_RGB565_BE 1 // 2 bytes/pixel, high byte first (LovyanGFX swap565_t)
#define ICON_BYTES (ICON_WIDTH * ICON_HEIGHT * 2)

#define ICON_REPLY_TAG 0xA0

typedef enum {
    ICON_CMD_BEGIN = 0x01,
    ICON_CMD_DATA = 0x02,
    ICON_CMD_END = 0x03,
    ICON_CMD_CLEAR = 0x04,
} icon_cmd_t;

typedef enum {
    ICON_ST_OK = 0,
    ICON_ST_BAD_STATE = 1,  // DATA/END without a BEGIN
    ICON_ST_BAD_PARAM = 2,  // unsupported size/format/length, or unknown command
    ICON_ST_BAD_OFFSET = 3, // out-of-order or overflowing DATA chunk
    ICON_ST_CRC_FAIL = 4,
    ICON_ST_INCOMPLETE = 5, // END before all bytes arrived
} icon_status_t;

void icon_store_init(void);

// Producer side: called from the TinyUSB task with one OUT report. Returns true if `reply`
// (ICON_HID_REPORT_SIZE bytes) was filled and should be sent back as an IN report.
bool icon_store_handle_report(const uint8_t *report, uint16_t len, uint8_t *reply);

// Consumer side (display task). Version increments on every commit or clear.
uint32_t icon_store_version(void);
// Copies the committed icon (ICON_BYTES, RGB565 big-endian) into dst. Returns false if no
// icon is set.
bool icon_store_copy(uint8_t *dst, size_t dst_len);
