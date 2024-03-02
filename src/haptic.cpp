#include "haptic.h"
#include "utils.h"

static const float idle_velocity_ewma_alpha = 0.001;
static const float idle_velocity_rad_per_sec = 0.05;
static const int32_t idle_correction_delay = 500; //ms

PIDController default_pid(1.22, 0, 0.004, 10000, 1.22);

hapticConfig default_config;
hapticParms default_params;



HapticState::HapticState(uint16_t position_num, float attract_distance){
    // intialize haptic state
    start_pos = 1;
    end_pos = position_num;
    total_pos = end_pos - start_pos +1;
    last_pos = 0;
    attract_angle = 0;
    last_attract_angle = 0;
    current_pos = 1;
    detent_count = 0;
    distance_pos = attract_distance * _PI / 180;
    detent_strength_unit = 0.5;
    endstop_strength_unit = 0.3;
    click_strength = 0.4;
};


HapticState::~HapticState() {};





HapticInterface::HapticInterface(BLDCMotor* _motor){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_config = default_config; // copy default to active configuration
    haptic_params = &default_params; // TODO this struct is not used??
    haptic_state = HapticState(haptic_config.position_num, haptic_config.attract_distance);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, hapticConfig* _config){
    motor = _motor;
    haptic_pid = &default_pid;
    haptic_config = *_config;
    haptic_params = &default_params; // TODO this struct is not used??
    haptic_state = HapticState(haptic_config.position_num, haptic_config.attract_distance);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = default_config; // copy default to active configuration
    haptic_params = &default_params; // TODO this struct is not used??
    haptic_state = HapticState(haptic_config.position_num, haptic_config.attract_distance);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = *_config; // copy over to active configuration
    haptic_params = &default_params; // TODO this struct is not used??
    haptic_state = HapticState(haptic_config.position_num, haptic_config.attract_distance);
};

HapticInterface::HapticInterface(BLDCMotor* _motor, PIDController* _pid, hapticConfig* _config, hapticParms* _params){
    motor = _motor;
    haptic_pid = _pid;
    haptic_config = *_config;
    haptic_params = &default_params; // TODO this struct is not used??
    haptic_state = HapticState(haptic_config.position_num, haptic_config.attract_distance);
};

void HapticInterface::init(void){
    motor->velocity_limit = 10000;
    motor->controller = MotionControlType::torque;
    motor->foc_modulation = FOCModulationType::SpaceVectorPWM;
};




void HapticInterface::find_detent(void)
{
    haptic_state.attract_angle = round(motor->shaft_angle / haptic_state.distance_pos) * haptic_state.distance_pos;
}

float HapticInterface::haptic_target(void)
{
    // TODO: When out of bounds and return to position introduce easing so we avoid overshoot.
    float error = haptic_state.last_attract_angle - motor->shaft_angle;
    
    // Call FOC loop
    motor->loopFOC();

    // Target Command
    if(fabsf(motor->shaft_velocity) > 60) {
        motor->move(0);
    // If velocity (turn speed is greather than x then dont apply any torque
    } else {
        motor->move(default_pid(error));
    }
    return haptic_pid->operator()(error);
}

void HapticInterface::haptic_click(void){
    float click_strength = haptic_state.click_strength;
    motor->move(click_strength);
    for(uint8_t i = 0; i< 3; i++){
        motor->loopFOC();
        delay(1);
    }
    motor->move(-click_strength);
    for(uint8_t i=0; i<3; i++){
        motor->loopFOC();
        delay(1);
    }
    motor->move(0);
}

void HapticInterface::haptic_loop(void){
    
    // TODO: Include Check if Config is Correct before triggering loop

    while (1){
    correct_pid(); // Adjust PID (Derivative Gain)
    find_detent(); // Calculate attraction angle depending on configured distance position.
    state_update(); // Determine and update current position    
    haptic_target(); // PID Command
    
    delayMicroseconds(1500); //Small delay between 750-1500us default 1500us helps with click response between calculations
    }
}

void HapticInterface::correct_pid(void)
{
    bool bound;
    // Derivative upper and lower strength are very small in estimated current mode, 10x in voltage mode.
    float d_lower_strength = haptic_state.detent_strength_unit * 0.010;
    float d_upper_strength = haptic_state.detent_strength_unit * 0.004;
    float d_lower_pos_width = radians(3);
    float d_upper_pos_width = radians(8);
    float raw = d_lower_strength + (d_upper_strength - d_lower_strength)/(d_upper_pos_width - d_lower_pos_width)*(haptic_state.distance_pos - d_lower_pos_width);

    haptic_pid->D = haptic_state.detent_count > 0 ? 0 : CLAMP(
        raw,
        min(d_lower_strength, d_upper_strength),
        max(d_lower_strength, d_upper_strength)
    );

    // Check if within range and apply voltage/current limit.
    if (haptic_state.attract_angle <= haptic_state.last_attract_angle - haptic_state.distance_pos || haptic_state.attract_angle >= haptic_state.last_attract_angle + haptic_state.distance_pos){
        bound = true;
    } else {
        bound = false;
    }
    haptic_pid->limit = bound ? haptic_state.endstop_strength_unit : haptic_state.detent_strength_unit;
}

void HapticInterface::state_update(void)
{
    // Determine and Update Current Position
    // Check if attractor angle is within calculated position between min and max position and update current position if in range.
    if(haptic_state.attract_angle > haptic_state.current_pos * haptic_state.distance_pos && haptic_state.current_pos < haptic_state.end_pos){
        haptic_state.current_pos++;
        haptic_state.last_attract_angle = haptic_state.attract_angle;

    } else if (haptic_state.attract_angle < haptic_state.current_pos * haptic_state.distance_pos && haptic_state.current_pos > haptic_state.start_pos){
        haptic_state.current_pos--;
        haptic_state.last_attract_angle = haptic_state.attract_angle;

    }
};

