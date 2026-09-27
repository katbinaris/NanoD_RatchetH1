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
    AUDIO_CLICK_NORMAL = 0,   // detent-crossing click (see CLICK_TIMBRE in i2s_task.c)
    AUDIO_CLICK_BUTTON_THUMP, // discrete button-tap confirmation -- a distinct, lower
                              // "thump" so it doesn't sound like a detent crossing
} audio_click_type_t;

void audio_trigger_init(void);

// Producer side (Core 0). Queues one click of the given type.
void audio_trigger_click(audio_click_type_t type);

// Consumer side (Core 1 only -- not safe for multiple consumers). Returns true and fills
// *out_type if there is at least one queued click not yet consumed; false if none pending.
// Each call consumes exactly one, oldest first -- callers wanting to pace playback (e.g.
// waiting for the previous click's minimum airtime before starting the next) should call
// this once per audio sample/tick and act on it only when ready to start a new one.
bool audio_trigger_try_consume(audio_click_type_t *out_type);
