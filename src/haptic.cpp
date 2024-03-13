#include "haptic.h"
#include "utils.h"

PIDController default_pid(1.22, 0, 0.004, 10000, 1.0);

hapticConfig default_config;

HapticState::HapticState(uint16_t position_num){
    // intialize haptic state
    detent_config.mode = HapticMode::REGULAR;
    detent_config.detent_count = position_num;
    detent_config.vernier = 10;
    detent_config.start_pos = 0;
    detent_config.end_pos = position_num;

    current_pos = 0;
    last_pos = 0;

    attract_angle = 0.0;
    last_attract_angle = 0.0;

    detent_strength_unit = 0.5;
    endstop_strength_unit = 0.3;
};

HapticState::~HapticState() {};

HapticInterface::HapticInterface(BLDCMotor* _motor){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_config = default_config; // copy default to active configuration
    haptic_state = HapticState(haptic_config.position_num);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, hapticConfig* _config){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_config = *_config;
    haptic_state = HapticState(haptic_config.position_num);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = default_config; // copy default to active configuration
    haptic_state = HapticState(haptic_config.position_num);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = *_config; // copy over to active configuration
    haptic_state = HapticState(haptic_config.position_num);
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
    float detent_width = haptic_state.detent_config.detent_count * _PI / 180;

    switch (haptic_state.detent_config.mode)
    {
    case HapticMode::REGULAR:
        break;

    case HapticMode::VERNIER:
        detent_width /= haptic_state.detent_config.vernier;

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

    uint16_t total_positions = haptic_state.detent_config.end_pos - haptic_state.detent_config.start_pos + 1;
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

void HapticInterface::find_detent(void)
{
    /**
     * Separate positive and negative error so that we can have asymmetric haptic textures.
     * Use this for when your fine and coarse detents are straddling the current detent.
     * */ 
    float detent_width = haptic_state.detent_config.detent_count * _PI / 180;
    float vernier_width  = detent_width / haptic_state.detent_config.vernier;

    if(haptic_state.attract_angle > motor->shaft_angle){
        // Knob is turned less than detent (left half of texture graph)
        switch(haptic_state.detent_config.mode){
        case HapticMode::REGULAR:
            // We only handle coarse decrements in this mode
            haptic_state.attract_angle = round(motor->shaft_angle / detent_width);
            haptic_state.attract_angle *= detent_width;
            break;

        case HapticMode::VERNIER:
            // fine -> coarse detent decrement
            haptic_state.attract_angle = round(motor->shaft_angle / vernier_width);
            haptic_state.attract_angle *= vernier_width;

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
        switch(haptic_state.detent_config.mode){
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

    if(haptic_state.last_attract_angle != haptic_state.attract_angle){
       if(haptic_state.last_attract_angle > haptic_state.attract_angle){
            if(haptic_state.current_pos > haptic_state.detent_config.start_pos){
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
       else{
            if(haptic_state.current_pos < haptic_state.detent_config.end_pos){
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
        if (!haptic_state.atLimit){
            HapticEventCallback(HapticEvt::EITHER);
        }
    }
}

float HapticInterface::haptic_target(void)
{
    // TODO: When out of bounds and return to position introduce easing so we avoid overshoot.
    float error = haptic_state.last_attract_angle - motor->shaft_angle;
    
    motor->loopFOC();

    // Prevent knob velocity from getting too high and overshooting.
    if(fabsf(motor->shaft_velocity) > 60) {
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
void UserHapticEventCallback(HapticEvt event, float currentAngle, uint16_t currentPos){
    /**
     * This function should not be modified.
     * To use this, implement the function in your main application.
     * extern "C" void HapticEventCallback(HapticEvt event, uint16_t currentPos)
    */
}