#ifndef HAPTIC_H
#define HAPIC_H

#include <Arduino.h>
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/mt6701/MagneticSensorMT6701SSI.h"
#include <motor.h>

typedef struct
{
    
    // Default Haptic Program Configuration
  
    
    uint8_t start_pos = 1; // Min Pos Index 
    uint16_t end_pos = 60; // Max Pos Index
    uint16_t total_pos = end_pos - start_pos +1;
    uint16_t detent_count = 0; //??
    uint16_t last_pos = 0; // Record last position for a recall
    float attract_angle = 0; // angle where PID will point to
    float last_attract_angle = 0;
    uint16_t current_pos = 1; // Current Actual Position
    float distance_pos = (360 / end_pos) * _PI / 180; // Define attractior distance position and convert it to radians
    float click_strength = 0.6; //  PID (estimated) Current Limit
    float detent_strength_unit =  0.6; // PID (estimated) Current Limit
    float endstop_strength_unit = 0.35; // PID (estimated) Current Limit
} hapticState;


typedef struct
{

    // Exponentially Weighted Moving Average (EWMA)
    // Used to correct PID, if position near alignment centre
    // but not there yet, adjust slowly to the centre
    // NOT USING

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

class HapticInterface
{
public:    
    hapticState* haptic_config;
    hapticParms* haptic_params;
    BLDCMotor* motor;
    PIDController* haptic_pid;

    // All the various constructors.
    HapticInterface(BLDCMotor* _motor);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid);
    HapticInterface(BLDCMotor* _motor, hapticState* _config);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticState* _config);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticState* _config, hapticParms* _parms);


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

#endif