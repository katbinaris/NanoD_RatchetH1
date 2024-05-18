
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

/**
 * Hooked up to HapticEventCallback, these are generated whenever there is a change
 * in the abstracted haptic state. 
*/
typedef enum : uint8_t {
    INCREASE = 0,   // When fine detents increment
    DECREASE = 1,   //..
    EITHER = 2,     // Fires anytime a coarse detent is reached
    LIMIT_POS = 3,  // At detent.end_pos
    LIMIT_NEG = 4   // At detent.start_pos
} HapticEvt;

/**
 * Supported haptic texture modes.
 * In regular, the number of detents is determined by end - start position, and the angle is set by detent_count (per 2PI)
 * In vernier mode, the number of detents is multiplied by the vernier multiplier, with "major" clicks where the regular detents would overlay.
 * In viscose mode, the knob is smooth but heavy and resists motion.
 * In spring mode, the knob returns to a defined point.
*/
typedef enum : uint8_t {
    REGULAR = 0,    //Only coarse detents used
    VERNIER = 1,    // Coarse with fine between
    VISCOSE = 2,    // Resistance while turning
    SPRING = 3     // Snap back to center point
} HapticMode;

/**
 * Defines the actual behavior of the detent profile.
 * Setting kxForce changes the feel of the detents so that larger values require larger force.
*/
typedef struct {
    HapticMode mode;
    uint16_t start_pos;
    uint16_t end_pos;
    uint16_t detent_count;
    uint8_t vernier;
    bool kxForce;
} DetentProfile;

/**
 * This is used for regular reporting of the knob mechanical state
 * if you are using the library as part of a larger system.
*/
typedef struct {
    uint8_t start_pos;
    uint16_t end_pos;
    uint16_t cur_pos;
} AngleEvt;
