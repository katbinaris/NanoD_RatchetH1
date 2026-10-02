#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// This fork's personal settings: the idle-screen text and the LED look (LIGHTS).
//
// LIGHTS follows menu.c's settings model -- live atomics, a saved copy, F2 saves, leaving the
// screen without F2 puts them back -- so menu.c captures/restores them with lights_get/set and
// saves them through lights_save(). The idle text has no on-device editor: the host sets it
// (tools/quadra.py text) and it is stored at once.

void user_prefs_init(void); // once, before the display and LED tasks start (loads NVS)

// --- Idle text: replaces the QUADRA wordmark (loading screen and idle animation) ---
#define USER_TEXT_MAX 12 // characters; printable ASCII, anything else becomes a space
uint32_t user_text_version(void);        // bumps on every change; any core
void user_text_get(char *out, size_t n); // "" = the stock wordmark
bool user_text_set(const char *s);       // live + NVS (a flash write: Core 1 only)

// --- LIGHTS: the LED ring and key LEDs ---
typedef enum { LIGHT_SRC_APP = 0, LIGHT_SRC_CUSTOM, LIGHT_SRC_COUNT } light_src_t;
typedef enum {
    LIGHT_FX_GRADIENT = 0, // the stock look: the palette around the ring, drifting while idle
    LIGHT_FX_SOLID,
    LIGHT_FX_BREATHE,
    LIGHT_FX_SPIN,         // a comet chasing round the ring
    LIGHT_FX_RAINBOW,      // the whole hue wheel, turning (ignores the colour)
    LIGHT_FX_OFF,          // dark at rest; the knob spot, flashes and notifications still show
    LIGHT_FX_COUNT
} light_fx_t;

typedef struct {
    int32_t src;   // light_src_t: the active app's colours, or hue/sat below
    int32_t hue;   // 0..359
    int32_t sat;   // 0..100 (0 = white)
    int32_t fx;    // light_fx_t
    int32_t speed; // 1..10 (BREATHE, SPIN, RAINBOW)
    int32_t level; // brightness, % of the stock level, 10..200 (power budget still applies)
} lights_t;

#define LIGHT_HUE_STEP 5
#define LIGHT_SAT_STEP 10
#define LIGHT_LEVEL_STEP 10

void lights_get(lights_t *out);      // any core
void lights_set(const lights_t *in); // clamps every field; any core
bool lights_save(void);              // the live values -> NVS (Core 1)

// Menu field callbacks (menu.c): rotate on Core 0 (CONTROL_HOT), format on Core 1.
void lights_rotate_src(int8_t dir);
void lights_rotate_hue(int8_t dir);
void lights_rotate_sat(int8_t dir);
void lights_rotate_fx(int8_t dir);
void lights_rotate_speed(int8_t dir);
void lights_rotate_level(int8_t dir);
const char *lights_fx_name(int32_t fx);
bool lights_custom(void);      // src == CUSTOM (Core 0 safe: the menu's mute callbacks)
bool lights_fx_animated(void); // the effect uses SPEED

// The custom colour as RGB888 at full value (hue/sat only), for swatches and the LED palette.
uint32_t lights_hsv(float hue_deg, float sat, float val);
