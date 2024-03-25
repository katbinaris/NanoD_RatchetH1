
#pragma once

#include <inttypes.h>
#include "common/foc_utils.h"

/*
 * Haptic API
 *
 * Common data structures needed by both comms and haptic modules
 * to effect the exchange of haptic configuration data.
 * 
 * The goal is to keep comms related complexity out of the haptic module
 * and motor related code and internals of the haptic module out of the comms module.
 * 
 * All data structures related to keys, leds or midi are removed from the haptic api,
 * as they have no relevance to the haptic module.
 */

typedef enum {
    INCREASE, // When fine detents increment
    DECREASE,  //..
    EITHER,   // Fires anytime a coarse detent is reached
    LIMIT_POS,  // At detent.end_pos
    LIMIT_NEG   // At detent.start_pos
} HapticEvt;

typedef enum {
    REGULAR,    //Only coarse detents used
    VERNIER,    // Coarse with fine between
    VISCOSE,    // Resistance while turning
    SPRING      // Snap back to center point
} HapticMode;

typedef struct {
    HapticMode mode;
    uint16_t start_pos;
    uint16_t end_pos;
    uint16_t detent_count;
    uint8_t vernier;
} DetentProfile;

typedef struct {
    float angle;
    int32_t turns;
    float velocity;
} AngleEvt;
