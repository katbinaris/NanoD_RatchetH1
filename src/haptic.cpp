#include "haptic.h"
#include "haptic_api.h"
#include "utils.h"

/*
    Quick Swap between Legacy PCB and Production PCB
*/
#define PRODUCTION_PCB 1

/**
 * Handful of default profiles that you can quickly explore.
*/


PIDController default_pid(5.0, 0.0, 0.004, 10000, 0.4);

DetentProfile default_profile{
    .mode = HapticMode::REGULAR,
    .start_pos = 0,
    .end_pos =255,
    .detent_count = 60,
    .vernier = 5,
    .kxForce = false
};

DetentProfile DefaultProgressiveForceProfile{
    .mode = HapticMode::REGULAR,
    .start_pos = 0,
    .end_pos = 120,
    .detent_count = 20,
    .vernier = 10,
    .kxForce = false
};

DetentProfile DefaultVernierProfile{
    .mode = HapticMode::VERNIER,
    .start_pos = 0,
    .end_pos = 120,
    .detent_count = 20,
    .vernier = 10,
    .kxForce = true
};

// Custom profile to implement and change externally.
extern DetentProfile tempProfile;

/**
 * Constructors and deconstructors.
*/
HapticState::HapticState(void){
    load_profile(default_profile, default_profile.start_pos);
};

HapticState::HapticState(DetentProfile profile){
    load_profile(profile, profile.start_pos);
};

HapticState::HapticState(DetentProfile profile, uint16_t position){
    load_profile(profile, position);
};

void HapticState::load_profile(DetentProfile profile, uint16_t new_position = 0xFFFF){
    
    uint16_t isVernier = profile.mode == HapticMode::VERNIER ? profile.vernier : 1;
   
    num_detents = profile.end_pos - profile.start_pos;
    detent_width = _2PI / profile.detent_count;

    if(profile.mode == HapticMode::VERNIER)
        detent_width /= profile.vernier;

    if(new_position != 0xFFFF)
        current_pos = new_position;
    else
    {
        // If no special handling of new position, check that we are in a valid region.
        if(current_pos < profile.start_pos * isVernier)
            current_pos = profile.start_pos;
        else if(current_pos > profile.end_pos * isVernier)
            current_pos = profile.end_pos;
    }

    last_pos = current_pos;
    detent_profile = profile;
}

HapticState::~HapticState() {};

HapticInterface::HapticInterface(BLDCMotor* _motor){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_state = HapticState(default_profile);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid){
    motor = _motor;
    haptic_pid = _pid;
    haptic_state = HapticState(default_profile);
};

void HapticInterface::init(void){
    motor->velocity_limit = 10000;
    motor->controller = MotionControlType::torque;
    motor->foc_modulation = FOCModulationType::SpaceVectorPWM;
};

void HapticInterface::haptic_loop(void){
    correct_pid(); // Adjust PID (Derivative Gain)
    find_detent(); // Calculate attraction angle depending on configured distance position.
    haptic_target(); // PID Command
}

/**
 * Handles scaling the P term error and clamping error to prevent overshoot.
 * The scaled P error helps to prevent steady state error due to lack of I term (for "rolling" reasons).
 * When roll mode is triggered, I term is added back in.
*/
void HapticInterface::correct_pid(void)
{
    bool clipping;

    // Derivative upper and lower strength are very small
    float d_lower_strength = haptic_state.detent_strength_unit * 0.008;
    float d_upper_strength = haptic_state.detent_strength_unit * 0.004;
    float d_lower_pos_width = radians(3);
    float d_upper_pos_width = radians(8);
    float raw = d_lower_strength;
    raw += (d_upper_strength - d_lower_strength)/(d_upper_pos_width - d_lower_pos_width);
    raw *= haptic_state.detent_width - d_lower_pos_width;
    

    uint16_t total_positions = haptic_state.detent_profile.end_pos - haptic_state.detent_profile.start_pos + 1;
    // If the error is large, clamp the D term so we don't overdrive the response.
    haptic_pid->D = total_positions > 0 ? 0 : CLAMP(
        raw,
        min(d_lower_strength, d_upper_strength),
        max(d_lower_strength, d_upper_strength)
    );

    // Check if within range and apply voltage/current limit.
    if (haptic_state.attract_angle <= haptic_state.last_attract_angle - haptic_state.detent_width)
        clipping = true;

    else if (haptic_state.attract_angle >= haptic_state.last_attract_angle + haptic_state.detent_width)
        clipping = true;

    else
        clipping = false;

    haptic_pid->P = clipping ? haptic_state.endstop_strength_unit : haptic_state.detent_strength_unit;
    
    
    if(haptic_state.wasAtLimit)
    haptic_pid->P = 0;
}

/** 
 * Utility function used for handling detent profile changes
*/
void HapticInterface::offset_detent(void){
    motor->sensor_offset = motor->shaft_angle;
    haptic_state.attract_angle = 0.0;
    haptic_state.last_attract_angle = 0.0;
}

/**
 * Handles motor level control of the detent (haptic texture map) and triggering 
 * updates into the haptic abstraction for detent index and bounding information.
 * This function could be greatly simplified if you don't need to entertain the idea of 
 * an asymmetric texture map, but I didn't want to lock out any functionality yet. 
 * 
 * Separate positive and negative error so that we can have asymmetric haptic textures.
 * Use this for when your fine and coarse detents are straddling the current detent.
*/
void HapticInterface::find_detent(void)
{
    /**
     * hysteresisType is used to handle whether the detents are linear in strength or progressively stronger.
     * We then generate new bounds for the detent based on the hysteresis (as a percentage)
    */
    float detentHysteresis;
    float minHysteresis;
    float maxHysteresis;

    // Special handling for progressive force modes.
    if(!haptic_state.detent_profile.kxForce){
        detentHysteresis = haptic_state.detent_width * haptic_state.attract_hysteresis;
        minHysteresis = haptic_state.attract_angle - detentHysteresis;
        maxHysteresis = haptic_state.attract_angle + detentHysteresis;
    }
    else{
        // 0.1 is a correction factor to compensate for the large hysteresis in normal modes, to make the force per detent reasonable.
        detentHysteresis = 0.15 * haptic_state.attract_hysteresis * -1.0;
        minHysteresis = haptic_state.attract_angle * (1.0 - detentHysteresis);
        maxHysteresis = haptic_state.attract_angle * (1.0 + detentHysteresis);
    }

    if(motor->shaft_angle < minHysteresis){
        // Knob is turned less than detent (left half of texture graph)
        haptic_state.attract_angle = round(motor->shaft_angle / haptic_state.detent_width); 
        haptic_state.attract_angle *= haptic_state.detent_width;
    }
    else if(motor->shaft_angle > maxHysteresis){
        // Knob is turned more than detent (right half of texture graph)
        haptic_state.attract_angle = round(motor->shaft_angle / haptic_state.detent_width);
        haptic_state.attract_angle *= haptic_state.detent_width;
    }

    // If there has been a change in the haptic attractor
    if(haptic_state.last_attract_angle != haptic_state.attract_angle){
        detent_handler();
    }
}

/**
 * If there has been an update in the physical location of the detent, this function helps handle
 * the abstraction in terms of detent index and firing off HapticEventCallback.
*/
void HapticInterface::detent_handler(void){
    // Logic for handling detent update events.
    uint16_t effective_start_pos = haptic_state.detent_profile.start_pos;
    uint16_t effective_end_pos = haptic_state.detent_profile.end_pos;

    if(haptic_state.detent_profile.mode == HapticMode::VERNIER){
        effective_start_pos *= haptic_state.detent_profile.vernier;
        effective_end_pos *= haptic_state.detent_profile.vernier;
    }

    // Check if we are increasing or decreasing detent
    if(haptic_state.last_attract_angle > haptic_state.attract_angle){

        #if PRODUCTION_PCB
        if(motor->sensor_direction == Direction::CCW){
        #else 
        if(motor->sensor_direction != Direction::CCW){
        #endif
            // Check that we are at limit
            if(haptic_state.current_pos > effective_start_pos){
                if(haptic_state.atLimit)
                    haptic_state.wasAtLimit = true;

                haptic_state.atLimit = false;
                haptic_state.current_pos--;
                haptic_state.last_attract_angle = haptic_state.attract_angle;

                HapticEventCallback(HapticEvt::DECREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_NEG);  
                haptic_state.atLimit = true;
            }
        }
        else{
            // Check that we are at limit
            if(haptic_state.current_pos < effective_end_pos){
                if(haptic_state.atLimit)
                    haptic_state.wasAtLimit = true;
                                   
                haptic_state.atLimit = false;
                haptic_state.current_pos++;
                haptic_state.last_attract_angle = haptic_state.attract_angle;

                HapticEventCallback(HapticEvt::INCREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_POS);  
                haptic_state.atLimit = true;
            }
        }
    
    }
    else{
        #if PRODUCTION_PCB
        if(motor->sensor_direction == Direction::CCW){
        #else
        if(motor->sensor_direction != Direction::CCW){
        #endif
            // Check if we are at limit
            if(haptic_state.current_pos < effective_end_pos){
                if(haptic_state.atLimit)
                    haptic_state.wasAtLimit = true;
                    
                haptic_state.atLimit = false; 
                haptic_state.current_pos++;   
                haptic_state.last_attract_angle = haptic_state.attract_angle;

                HapticEventCallback(HapticEvt::INCREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_POS);
                haptic_state.atLimit = true;
                haptic_state.wasAtLimit = false;
            }
        }
        else{
            // Check if we are at limit
            if(haptic_state.current_pos > effective_start_pos){
                if(haptic_state.atLimit)
                    haptic_state.wasAtLimit = true;

                haptic_state.atLimit = false;  
                haptic_state.current_pos--;  
                haptic_state.last_attract_angle = haptic_state.attract_angle;

                HapticEventCallback(HapticEvt::DECREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_NEG);
                haptic_state.atLimit = true;
            }
        }
    }

    if (!haptic_state.atLimit){
        HapticEventCallback(HapticEvt::EITHER);
    }
}

/**
 * Handling the position error and limit to within a detent, as well as clearing wasAtLimit flag.
*/
void HapticInterface::haptic_target(void)
{
    float error = haptic_state.last_attract_angle - motor->shaft_angle;
    float error_threshold = haptic_state.detent_width * 0.0075; // 0.75% gives good snap without ringing
    // default_pid.output_ramp = haptic_state.detent_profile.output_ramp;
    haptic_pid->output_ramp = haptic_state.detent_profile.output_ramp;
     // Prevent knob velocity from getting too high and overshooting.
    // If the position error is small, reduce strength to prevent oscillation
    if(fabsf(error) < error_threshold)
        error *= 0.75;

    else if(fabsf(motor->shaft_velocity) > 30){

        // Prevent knob velocity from getting too high and overshooting.
        // Low values of this actually provide pretty interesting feeling where knob is smooth while 
        // rotating quickly by hand but snappy during fine adjust.
        error = 0.0;
    }
    else {
        error = CLAMP(
            error,
            -haptic_state.detent_width,
            haptic_state.detent_width
        );
    }

    // When re-entering valid bounds, quickly try to get back to attract angle without involving haptics to prevent sliding into a further detent
    if(!haptic_state.atLimit && haptic_state.wasAtLimit){
        bounds_handler(haptic_state.detent_width);
    }
    else
        motor->loopFOC();
        motor->move(default_pid(error));
}

/**
 * Handles the transition from out of bounds to in bounds movement 
 * to prevent overshooting by escaping from the haptic loop to settle the response.
*/
void HapticInterface::bounds_handler(float detent_width)
{
    float error = 0.0;

    while(fabsf(motor->shaft_velocity) > 1.0){
        error = haptic_state.attract_angle - motor->shaft_angle;
        // If you are driving the motor by hand, skip out of here so that you don't feel dragging on the knob
        if(fabsf(error) > (detent_width * 2))
            break;

        motor->loopFOC();
        motor->move(default_pid(error));
    }

    // Determine current boundary
    if(haptic_state.current_pos <= haptic_state.num_detents / 2)
        haptic_state.current_pos = haptic_state.detent_profile.start_pos;
    else
        haptic_state.current_pos = haptic_state.detent_profile.end_pos;

    // Correct position scaling if in vernier mode.
    if(haptic_state.detent_profile.mode == HapticMode::VERNIER)
        haptic_state.current_pos *= haptic_state.detent_profile.vernier;

    // Fix physical drifting due to missing a detent when re-entering bounds.
    if(haptic_state.current_pos <= haptic_state.num_detents / 2)
        haptic_state.current_pos += 1;
    else
        haptic_state.current_pos -= 1;

    // Clear boundary exit flag
    haptic_state.wasAtLimit = false;
}
// Internal detent update handler.
void HapticInterface::HapticEventCallback(HapticEvt event){
    UserHapticEventCallback(event, motor->shaft_angle, haptic_state.current_pos);
}

// For user implementation
//  __attribute__((weak)) void UserHapticEventCallback(HapticEvt event, float currentAngle, uint16_t currentPos){
//     /**
//      * This function should not be modified.
//      * To use this, implement the function in your main application.
//      * extern "C" void HapticEventCallback(HapticEvt event, uint16_t currentPos)
//     */
// }