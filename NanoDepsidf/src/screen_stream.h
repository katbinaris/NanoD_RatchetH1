#pragma once

#include <stdbool.h>
#include <stdint.h>

// The screen, live, for the companion app (EXT_CMD_SCREEN): each frame the display pushes is
// compared with what the host already has, 16x16 tiles at a time, and only the changed tiles
// go out -- run-length coded when that's shorter (a menu, the clock: mostly flat colour), raw
// otherwise (a cover). A still screen costs nothing; a cover change is ~115 KB once.
//
// Stream (EXT_TAG_SCREEN reports, 60 bytes each): per frame, tile records
//   [tile 0..224][mode 0 raw / 1 RLE][length u16][data]
// raw: 256 pixels, RGB565 big-endian; RLE: (count 1..255, pixel RGB565 big-endian) triples.
// Tiles run left to right, top to bottom, as the user sees the screen (rotation applied).

#define SCREEN_W 240
#define SCREEN_H 240

void screen_stream_set(uint8_t fps); // 0 = off; a new start sends the whole screen first
// Display task: after a frame was pushed (`fresh`), or on any later tick to catch up with one
// that couldn't go then. `frame`: the sprite's pixels, RGB565 byte-swapped (as LovyanGFX keeps
// them -- the order the stream wants).
void screen_stream_frame(const uint16_t *frame, bool fresh);
// host_link (holding its TX lock): the next report of the frame being sent, if there is one;
// screen_stream_sent() once it went out.
bool screen_stream_next(uint8_t report[64]);
void screen_stream_sent(void);
void screen_stream_stop(void); // the host went away
