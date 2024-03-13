
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

typedef struct {

    // Feedback Settings

    uint8_t profile_type = 0; 

    /* 
        0 - Fine Detents (Torque Mode)
        1 - Coarse Detents (Torque Mode)
        2 - Viscous Friction (Velocity Mode) 
        3 - Return To Centre (Torque Mode)

        What is function of these?

        Profile Type [0,1] are not fulfilling any function in firmware, 
        these are just to be read by front-end and then UI configuration 
        options are adapted depending on type. Haptic Firmware handles the rest.

        Profile Type 0 - UI locks attractor angle range between 2-6 degrees
        Profile Type 1 - UI locks attractor angle range between 7-180 degrees

        ###

        Profile type [2] tells firmware switch to MotionController::Velocity
        Profile Type 2 - UI locks renames attraction angle to position distance and locks range between 1-12,
        On Firmware side this value is still defined as attract_distance

        ###

        Profile Type [3] tells firmware to send current position as negative
        or positive number which is rounded up degree in range of [-180 ... 180].
        In this mode the default position is always 0, and as knob is being rotated CW or CCW the angular difference
        CCW [-180...0]; CW [0...180]

        Profile Type 3 - UI limits position number to 1 and does not render users ability to modify attractor angle
    */

    uint16_t position_num = 12;

    /*
        Total number of defined positions 
        UI -> Total Positions
        For 'Fine Detents', 'Coarse Detents' and 'Viscous Friction' these values must be between [2...65535]
        For Return To Centre this is always 1 and if this mode is selected user cannot ented number of detents in Front-End UI
    */

    uint8_t attract_distance = 20; 

    /*
        Distance between virtual points represented in Degrees, these shouldn't be float values i think. 
        User is likely to never notice difference in decimal adjustment.
        Firmware then converts that vallue from dergees to radians.
    */

    uint8_t feedback_strength = 6;

    /*
        Feedback Strength modifies haptic_pid->limit, estimated current,
        Values from [1...6]
        Input values then are divided by factor of 10. Output should be decimal eg, 0.1 - 0.6 (A estimated)
        Note: In my experience there is really no need to make this more granular, 
        with current algorithm anything over 600mA renders feedback choppy 
        (Need to investigate maybe this is related to incorrect phase resistance should phase resistance be measured value or measured value / 2 eg)

        If it doesnt matter whether this value is int or float, maybe better to be able to send float instead
    */

    uint8_t bounce_strength = 3;

    /*
        Bounce Strength modifies haptic_pid->limit, estimated current,
        Values from [1...6]
        Input values then are divided by factor of 10. Output should be decimal eg, 0.1 - 0.6 (A estimated)

        If it doesnt matter whether this value is int or float, maybe better to be able to send float instead

    */

    uint8_t haptic_click_strength = 6;

    /* 
        Click Strength modifies hapic_pid->limit, estimated current.
        Values from [1...6]
        This is being used by HapticInterface::haptic_click function
    */

    uint16_t output_ramp = 10000;

    /*
        Feedback Strength modifies haptic_pid->output_ramp
        Values from [2...10] with steps every 2000 (no float values)
        Input values are then multiplied by factor of 10000.

    */
} hapticConfig;

