
#pragma once

#include <inttypes.h>

#define _PI 3.14159265358979323846f

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
 * 
 */

typedef struct {
    // Exponentially Weighted Moving Average (EWMA)
    // Used to correct PID, if position near alignment centre
    // but not there yet, adjust slowly to the centre
    // TODO: Implement EWMA and compare perfromance w/wo
    float dead_zone_percent = 0.2;
    float dead_zone_rad = _PI/180;

    float idle_velocity_ewma_alpha = 0.001;
    float idle_velocity_rad_per_sec = 0.05;
    float idle_check_velocity_ewma = 0;
    float idle_correction_max_angle_rad = 5 * _PI/180;
    float idle_correction_rate_alpha = 0.0005;

    float last_idle_start = 0;
    uint32_t idle_correction_delay_millis = 500;
} hapticParms;


