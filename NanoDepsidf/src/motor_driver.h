#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t motor_driver_init(void);
void motor_driver_enable(bool enable);

// Phase voltages in volts, referenced to a virtual neutral at MOTOR_MAX_VOLTAGE_V/2.
// Internally clamped to the physical supply rail -- does NOT enforce the current-derived
// safety limit itself, callers must clamp Vd/Vq to MOTOR_EFFECTIVE_STATIC_VOLTAGE_LIMIT_V or
// MOTOR_EFFECTIVE_ROTATING_VOLTAGE_LIMIT_V (whichever applies) first.
void motor_driver_set_phase_voltages(float ua, float ub, float uc);
