#pragma once

#include <Arduino.h>
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "encoders/mt6701/MagneticSensorMT6701SSI.h"
#include <motor.h>
#include "haptic_api.h"

/**
 * Define start and end positions. These are the physical detent limits.
 * Coarse index and fine index track where in that range the dial is currently pointing.
 * Coarse num and fine num are the number of detents for each region. 
 * 
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

class HapticState 
{
public:
    HapticState(void);
    HapticState(DetentProfile profile);
    ~HapticState();

    DetentProfile detent_profile;

    uint16_t current_pos = 0;
    uint16_t last_pos = 0; 

    float attract_angle = 0; 
    float last_attract_angle = 0;

    float detent_strength_unit = 0.5; // PID (estimated) Current Limit
    float endstop_strength_unit = 0.5; // PID (estimated) Current Limit

    bool atLimit = 0;

    void load_profile(DetentProfile);
};

class HapticInterface
{
public:
    hapticConfig haptic_config; // Haptic configuration
    HapticState haptic_state;   // Haptic state

    BLDCMotor* motor;
    PIDController* haptic_pid;

    // All the various constructors.
    HapticInterface(BLDCMotor* _motor);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid);
    HapticInterface(BLDCMotor* _motor, hapticConfig* _config);
    HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config);

    void init(void);
    void haptic_loop(void);
    void HapticEventCallback(HapticEvt);
    void UserHapticEventCallback(HapticEvt, float, uint16_t) __attribute__((weak));

private:
    void offset_detent(void);
    void find_detent(void);
    void update_position(void);
    float haptic_target(void);
    void correct_pid(void);
};
