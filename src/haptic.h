#pragma once

#include <Arduino.h>
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/mt6701/MagneticSensorMT6701SSI.h"
#include <motor.h>
#include "haptic_api.h"


class HapticState {
public:
    HapticState(uint16_t position_num, float attract_distance);
    ~HapticState();

    uint8_t start_pos = 1;
    uint16_t end_pos;
    uint16_t total_pos = end_pos - start_pos +1;

    uint16_t last_pos = 0; // Record last position for a recall
    float attract_angle = 0; // angle where PID will point to
    float last_attract_angle = 0;

    // Current Position Behavior
    uint16_t current_pos = 1; // Current Actual Position
    /* 
    Current position is always represented as integer
    */

    uint8_t detent_count = 0;

    float distance_pos; // Define attractior distance position and convert it to radians

    float detent_strength_unit = 0.5; // PID (estimated) Current Limit

    float endstop_strength_unit = 0.3; // PID (estimated) Current Limit

    float click_strength = 0.4; //  PID (estimated) Current Limit
};






class HapticInterface
{
public:   
    HapticState* haptic_state;
    hapticParms* haptic_params;
    hapticConfig* haptic_config;
    BLDCMotor* motor;
    PIDController* haptic_pid;

    // All the various constructors.
    HapticInterface(BLDCMotor* _motor);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid);
    HapticInterface(BLDCMotor* _motor, hapticConfig* _config);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config, hapticParms* _parms);


    void init(void);
    void change_haptic_mode(void);
    void haptic_loop(void);
    void haptic_click(void);

protected:
private:
    void find_detent(void);
    void update_position(void);
    void state_update(void);
    float haptic_target(void);
    void correct_pid(void);
};

