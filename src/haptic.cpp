#include "haptic.h"
#include "haptic_api.h"
#include "utils.h"

hapticConfig default_config;

PIDController default_pid(1.0, 0.1, 0.004, 10000, 1.2);

DetentProfile default_profile{
    .mode = HapticMode::REGULAR,
    .start_pos = 0,
    .end_pos = 30,
    .detent_count = 10,
    .vernier = 10
};

HapticState::HapticState(void){
    detent_profile = default_profile;
    current_pos = detent_profile.start_pos;
};

HapticState::HapticState(DetentProfile profile){
    detent_profile = profile;
    current_pos = detent_profile.start_pos;
};

void HapticState::load_profile(DetentProfile profile){
    detent_profile = profile;

    current_pos = detent_profile.start_pos;
    last_pos = detent_profile.start_pos;

    attract_angle = 0.0;
    last_attract_angle = 0.0;
}

HapticState::~HapticState() {};

HapticInterface::HapticInterface(BLDCMotor* _motor){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_config = default_config; // copy default to active configuration
    haptic_state = HapticState(default_profile);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, hapticConfig* _config){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_config = *_config;
    haptic_state = HapticState(default_profile);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = default_config; // copy default to active configuration
    haptic_state = HapticState(default_profile);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = *_config; // copy over to active configuration
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
    delayMicroseconds(1500); //Small delay between 750-1500us default 1500us helps with click response between calculations
}

void HapticInterface::correct_pid(void)
{
    bool clipping;
    float detent_width = 2 * _PI / haptic_state.detent_profile.detent_count;

    switch (haptic_state.detent_profile.mode)
    {
    case HapticMode::REGULAR:
        break;

    case HapticMode::VERNIER:
        detent_width /= haptic_state.detent_profile.vernier;

    case HapticMode::VISCOSE:
        break;

    case HapticMode::SPRING:
        break;

    default:
        break;
    }

    // Derivative upper and lower strength are very small in estimated current mode, 10x in voltage mode.
    float d_lower_strength = haptic_state.detent_strength_unit * 0.010;
    float d_upper_strength = haptic_state.detent_strength_unit * 0.004;
    float d_lower_pos_width = radians(3);
    float d_upper_pos_width = radians(8);
    float raw = d_lower_strength + (d_upper_strength - d_lower_strength)/(d_upper_pos_width - d_lower_pos_width)*(detent_width - d_lower_pos_width);

    uint16_t total_positions = haptic_state.detent_profile.end_pos - haptic_state.detent_profile.start_pos + 1;
    // If the error is large, clamp the D term so we don't overdrive the response.
    haptic_pid->D = total_positions > 0 ? 0 : CLAMP(
        raw,
        min(d_lower_strength, d_upper_strength),
        max(d_lower_strength, d_upper_strength)
    );

    // Check if within range and apply voltage/current limit.
    if (haptic_state.attract_angle <= haptic_state.last_attract_angle - detent_width){
        clipping = true;
    }
    else if (haptic_state.attract_angle >= haptic_state.last_attract_angle + detent_width){
        clipping = true;
    } 
    else{
        clipping = false;
    }

    haptic_pid->limit = clipping ? haptic_state.endstop_strength_unit : haptic_state.detent_strength_unit;
}

void HapticInterface::offset_detent(void){
    motor->sensor_offset = motor->shaft_angle;
    haptic_state.attract_angle = 0.0;
    haptic_state.last_attract_angle = 0.0;
}

void HapticInterface::find_detent(void)
{
    /**
     * Separate positive and negative error so that we can have asymmetric haptic textures.
     * Use this for when your fine and coarse detents are straddling the current detent.
     * */ 
    float detent_width = 2 * _PI / haptic_state.detent_profile.detent_count;
    float vernier_width  = detent_width / haptic_state.detent_profile.vernier;

    if(haptic_state.attract_angle > motor->shaft_angle){
        // Knob is turned less than detent (left half of texture graph)
        switch(haptic_state.detent_profile.mode){
        case HapticMode::REGULAR:
            // We only handle coarse decrements in this mode
            haptic_state.attract_angle = round(motor->shaft_angle / detent_width);
            haptic_state.attract_angle *= detent_width;
            break;

        case HapticMode::VERNIER:
            // fine -> fine detent decrement
            haptic_state.attract_angle = round(motor->shaft_angle / vernier_width);
            haptic_state.attract_angle *= vernier_width;               
            break;

        default:
            break;
        }
    }
    else{
        // Knob is turned more than detent (right half of texture graph)
        switch(haptic_state.detent_profile.mode){
        case HapticMode::REGULAR:
            // We only handle coarse decrements in this mode
            haptic_state.attract_angle = round(motor->shaft_angle / detent_width);
            haptic_state.attract_angle *= detent_width;
            break;

        case HapticMode::VERNIER:
            // fine -> fine detent increment
            haptic_state.attract_angle = round(motor->shaft_angle / vernier_width);
            haptic_state.attract_angle *= vernier_width;        
            break;

        default:
            break;
        }
    }

    // If there has been a change in the haptic attractor
    if(haptic_state.last_attract_angle != haptic_state.attract_angle){
        detent_handler();
    }
}

void HapticInterface::detent_handler(void){
    // Logic for handling detent update events.
    uint16_t effective_end_pos = haptic_state.detent_profile.end_pos;

    if(haptic_state.detent_profile.mode == HapticMode::VERNIER){
        effective_end_pos *= haptic_state.detent_profile.vernier;
    }

    // Check if we are increasing or decreasing detent
    if(haptic_state.last_attract_angle > haptic_state.attract_angle){
        if(motor->sensor_direction == Direction::CCW){
            // Check that we are at limit
            if(haptic_state.current_pos > haptic_state.detent_profile.start_pos){
                haptic_state.last_attract_angle = haptic_state.attract_angle;
                haptic_state.atLimit = 0;
                haptic_state.current_pos--;
                HapticEventCallback(HapticEvt::DECREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_NEG);                
                haptic_state.atLimit = 1;
            }
        }
        else{
            // Check that we are at limit
            if(haptic_state.current_pos < effective_end_pos){
                haptic_state.last_attract_angle = haptic_state.attract_angle;
                haptic_state.atLimit = 0;
                haptic_state.current_pos++;
                HapticEventCallback(HapticEvt::INCREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_POS);                
                haptic_state.atLimit = 1;
            }
        }

    }
    else{
        if(motor->sensor_direction == Direction::CCW){
            // Check if we are at limit
            if(haptic_state.current_pos < effective_end_pos){
                haptic_state.atLimit = 0;  
                haptic_state.current_pos++;              
                haptic_state.last_attract_angle = haptic_state.attract_angle;
                HapticEventCallback(HapticEvt::INCREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_POS);
                haptic_state.atLimit = 1;
            }
        }
        else{
            // Check if we are at limit
            if(haptic_state.current_pos > haptic_state.detent_profile.start_pos){
                haptic_state.atLimit = 0;  
                haptic_state.current_pos--;              
                haptic_state.last_attract_angle = haptic_state.attract_angle;
                HapticEventCallback(HapticEvt::DECREASE);
            }
            else{
                HapticEventCallback(HapticEvt::LIMIT_NEG);
                haptic_state.atLimit = 1;
            }
        }
    }

    if (!haptic_state.atLimit){
        HapticEventCallback(HapticEvt::EITHER);
    }
}

float HapticInterface::haptic_target(void)
{
    // TODO: When out of bounds and return to position introduce easing so we avoid overshoot.
    float error = haptic_state.last_attract_angle - motor->shaft_angle;

    motor->loopFOC();

    // Prevent knob velocity from getting too high and overshooting.
    if(fabsf(motor->shaft_velocity) > 30) {
        motor->move(0);
    } 
    else {
        motor->move(default_pid(error));
    }

    return haptic_pid->operator()(error);
}

// Internal detent update handler.
void HapticInterface::HapticEventCallback(HapticEvt event){
    UserHapticEventCallback(event, motor->shaft_angle, haptic_state.current_pos);
}

// For user implementation
void HapticInterface::UserHapticEventCallback(HapticEvt event, float currentAngle, uint16_t currentPos){
    /**
     * This function should not be modified.
     * To use this, implement the function in your main application.
     * extern "C" void HapticEventCallback(HapticEvt event, uint16_t currentPos)
    */
}