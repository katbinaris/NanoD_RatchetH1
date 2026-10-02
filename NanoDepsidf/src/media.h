#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// What's playing on the computer, from the host daemon (tools/mac/quadrad.py): the cover as
// a JPEG (decoded by the display task), the track, its colours and the system volume. The
// MUSIC profile's screen shows it; its LEDs take the cover's colours.

#define MEDIA_TEXT_MAX 24
#define MEDIA_COVER_MAX (64 * 1024) // bytes of JPEG; the host keeps a 240x240 cover under this

typedef struct {
    bool playing;
    int8_t volume;        // 0..100, -1 = unknown
    uint32_t palette[3];  // RGB888, the cover's colours (0 = none)
    char title[MEDIA_TEXT_MAX + 1];
    char artist[MEDIA_TEXT_MAX + 1];
} media_track_t;

// --- host side (TinyUSB task, Core 1) ---
void media_set_track(const media_track_t *t);
void media_clear(void); // nothing playing: back to the normal MUSIC screen
// Cover upload, in order. Returns false when the transfer is refused (no memory, too big,
// out of order); END verifies the length and CRC-32 before the cover becomes current.
bool media_cover_begin(uint32_t len, uint32_t crc);
bool media_cover_data(uint32_t offset, const uint8_t *p, uint32_t n);
bool media_cover_end(void);

// --- display / LEDs (Core 1) ---
bool media_get_track(media_track_t *out); // false = nothing playing
uint32_t media_version(void);              // bumps on any change (track, volume, cover)
// The current cover's JPEG, copied into `dst` (up to `cap` bytes). Returns its length, 0
// when there's no cover. `*version` gets the cover's version.
size_t media_cover_copy(uint8_t *dst, size_t cap, uint32_t *version);
uint32_t media_cover_version(void);
