#pragma once
#include <stdbool.h>

// Maps raw sensor angle -> true electrical angle: electrical_rad =
// wrap(direction * mechanical_rad * pole_pairs - electrical_offset_rad).
typedef struct {
    float electrical_offset_rad;
    float direction; // +1.0 or -1.0
    bool valid;
} foc_calibration_t;

// Blocking. Assumes motor_driver_init() already called and driver already enabled with
// phases at neutral. Aligns the rotor via direct-axis voltage, then takes a small step to
// detect sensor direction. Returns .valid=false (motor left at neutral, safe) if the rotor
// doesn't appear to move during the direction-detection step.
foc_calibration_t foc_calibration_run(void);

// NVS-backed persistence (namespace "foc_cal") so a mechanically unchanged sensor/motor
// pairing doesn't need to repeat the calibration jerks on every boot. Assumes
// nvs_flash_init() has already been called (done once in app_main()).
bool foc_calibration_load(foc_calibration_t *out);
void foc_calibration_save(const foc_calibration_t *cal);
