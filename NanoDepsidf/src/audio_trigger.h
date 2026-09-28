#pragma once

#include <stdint.h>
#include <stdbool.h>

// Cross-core haptic-click signaling: the control loop (Core 0, control_task.c) is the
// single producer, the I2S audio task (Core 1, i2s_task.c) is the single consumer.
// Modeled directly on Lucu-Kind/src/shared_state.h's g_clickTriggerCount pattern (see
// DEVELOPMENT_PLAN.md Phase 7) rather than a FreeRTOS queue: a monotonically incrementing
// counter, never overwritten, so 2+ detents landing inside one audio-task poll window
// still play as that many distinct clicks instead of collapsing into one (which a plain
// shared "trigger now" flag/float would do). Safe to call audio_trigger_click() from the
// real-time control loop -- it's a couple of atomic ops, no blocking, no locks.

typedef enum {
    AUDIO_CLICK_NORMAL = 0,   // detent-crossing click (timbre chosen via audio_click_timbre_t)
    AUDIO_CLICK_BUTTON_THUMP, // discrete button-tap confirmation -- a distinct, lower
                              // "thump" so it doesn't sound like a detent crossing, and NOT
                              // affected by audio_click_timbre_t (only the detent click has
                              // more than one timbre)
} audio_click_type_t;

// Phase 8 step 5: which DETENT click timbre i2s_task.c renders -- was a compile-time
// CLICK_TIMBRE #define there (step 7 audibility-test scaffolding), now the Haptic
// Configurator's "Haptic Sound" field (menu.c, menu_get_haptic_sound()) selects it live.
// Declared here (the shared audio header both menu.c and i2s_task.c already include) rather
// than in either of those files, same reasoning as haptic_params.h's haptic_type_t.
typedef enum {
    AUDIO_TIMBRE_WOOD_TOCK = 0, // fast-decay sine with a downward pitch chirp
    AUDIO_TIMBRE_TICK_THUD,     // sharp high tick layered with a low body thud
    AUDIO_TIMBRE_COUNT
} audio_click_timbre_t;

// Phase 8 step 5 (added while wiring the above): a live "Pitch" field alongside "Haptic
// Sound" in the Haptic Configurator, multiplying every frequency in the active DETENT
// timbre's oscillator(s) -- i2s_task.c's WOOD_TOCK_BASE_FREQ_HZ or
// TICK_THUD_TICK_FREQ_HZ/TICK_THUD_THUD_FREQ_HZ, whichever is currently selected. A single
// multiplier (not a per-timbre absolute Hz) so it applies uniformly regardless of which
// timbre is active and keeps a multi-partial timbre's partials in the same relative
// interval to each other when pitched. Does NOT affect the button-tap thump (a separate,
// fixed confirmation sound, not part of "timbre"). 0.5x/2.0x is +-1 octave.
#define AUDIO_CLICK_PITCH_DEFAULT 1.0f
#define AUDIO_CLICK_PITCH_MIN 0.5f
#define AUDIO_CLICK_PITCH_MAX 2.0f

void audio_trigger_init(void);

// Producer side (Core 0). Queues one click of the given type.
void audio_trigger_click(audio_click_type_t type);

// Consumer side (Core 1 only -- not safe for multiple consumers). Returns true and fills
// *out_type if there is at least one queued click not yet consumed; false if none pending.
// Each call consumes exactly one, oldest first -- callers wanting to pace playback (e.g.
// waiting for the previous click's minimum airtime before starting the next) should call
// this once per audio sample/tick and act on it only when ready to start a new one.
bool audio_trigger_try_consume(audio_click_type_t *out_type);
