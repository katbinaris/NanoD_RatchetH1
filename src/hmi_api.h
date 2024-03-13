
#pragma once 

#include <inttypes.h>


typedef struct  {
    uint8_t channel;
    uint8_t cc;
    uint8_t val;
} nanoMidiConfig;


typedef struct {
    uint8_t num;
    uint8_t key_codes[32];
} nanoKeyboardConfig;


typedef struct {

} nanoGamepadConfig;



typedef struct {

} nanoMouseConfig;



typedef enum {
    KA_NONE = 0,
    KA_KEY = 1,
    KA_MIDI = 2,
    KA_MOUSE = 3,
    KA_GAMEPAD = 4,
    KA_PROFILE_CHANGE = 5
} keyActionType;


typedef struct {
    keyActionType type;
    union {
        nanoMidiConfig midi;
        nanoKeyboardConfig hid;
        nanoMouseConfig mouse;
        nanoGamepadConfig pad;
    };
} keyAction;


typedef struct {
    uint8_t num_pressed_actions = 0;
    uint8_t num_held_actions = 0;
    uint8_t num_released_actions = 0;
    keyAction pressed[5];
    keyAction held[5];
    keyAction released[5];
} keyMapping;



typedef enum {
    KV_MOUSE = 1,
    KV_GAMEPAD = 2,
    KV_MIDI = 3,
    KV_ACTIONS = 4,
    KV_DEVICE_PROFILES = 5 
} knobValueType;



typedef struct {
    keyAction every;
    keyAction cw;
    keyAction ccw;
} knobActionsConfig;



typedef struct {
    // TODO: Add haptic feedback configuration
    float angleMin; // minimum angle of the knob, inclusive
    float angleMax; // maximum angle of the knob, inclusive, unless wrap is true, then exclusive
    bool wrap; // if true, the angle will be allowed to wrap from min to max and vice versa
               // if false, the motor will return to the min/max if released outside the range
    uint32_t detents;
} knobHapticConfig;



typedef struct {
    uint8_t key_state;
    knobValueType type;
    float value_min;    // minumum value returned by the knob, inclusive
    float value_max;    // maximum value returned by the knob, inclusive unless wrap is true, then exclusive
    float angle_min;    // minimum angle of the knob, inclusive
    float angle_max;    // maximum angle of the knob
    bool max_exclusive; // if true, then angle_max is exclusive, otherwise it is inclusive
    bool wrap;          // if true, the value will wrap around from min to max and vice versa    
    uint32_t steps;     // quantize the values output with the given number of steps
                        // note: if steps is 0, the value is continuous
                        //       if steps is 1, integer values are returned
    bool recenter;      // if true, angle_min and angle_max are recalculated to be centred around the current angle
                        // if false, angle_min and angle_max are used as provided
    
    // note: if min > max for either value or angle, the value will be negatively correlated to the knob angle
    // note: if wrap is false, and the knob is turned outside the min-max range, the value will be clamped to the min-max range
    // note: if wrap is true, and angle_max-angle_min is not a multiple of 2PI, there will be a dead zone between 
    //       the max and the min angles, where the value will not change
    // note: if wrap is true, and angle_max-angle_min is a multiple of 2PI, the angle_max value will always be exclusive, regardless
    //       of the value of maxExclusive, and there will be no dead zone as there is no gap between max and min angles

    knobHapticConfig haptic;
    union {
        nanoMouseConfig mouse;
        nanoGamepadConfig pad;
        nanoMidiConfig midi;
        knobActionsConfig actions;
    };
} knobValue;



typedef struct {
    uint8_t num = 0;
    knobValue values[8];
} knobMapping;


typedef struct {
    keyMapping keys[4];
    knobMapping knob;
} hmiConfig;



typedef struct {
    uint8_t type;
    uint8_t keyNum;
    uint8_t keyState;
} KeyEvt;




//
// Older notes
//



    /* 

        Context:
        Mapping Buttons can play few roles:

        Case 1: Mapped Button can trigger another profile (eg. pres button -> go to next profile withing same group [tag] or go to specific profile [uid])
        Case 2: Mapped Button can toggle current/active CC # - knob sends values 0-127 the output message
        Case 3: Mapped Button can send specific CC # with specific value
        Case 4: Mapped Button contains mapping value that can be parsed by RobotJS (eg. Key 'B' or 'Arrow Up' or 'Ctrl')
        TODO: how mappings for buttons and knob should be stored as?

    */

    // bool internal_macro = false; // If true, button is used to trigger internal macro

    // String knob_map = "1016001255"; //  MODE: Midi(1), TRIGGER: Press(0), MIDI CH (16), CC#(01), CC(255)
    // String button_A_map = "1016001255"; //  MODE: Midi(1), TRIGGER: Press(0), MIDI CH (16), CC#(01), CC(255)
    // String button_B_map = "214567"; // MODE: Internal(2), TRIGGER: Hold(1), UID: (4567)
    // String button_C_map = "0"; // No mapping -> Disables button and RGB assigned for that button
    // String button_D_map = "0"; //etc.

    /*
    Each button mapping should contain list of parameters:
        Parameter 0 - Type of Mapping (?)
        Parameter 1 - Mapping Trigger (Eg. Single Press or Hold)
        Parameter 2 - Mapping Type (Eg. Internal, Virtual HID, MIDI)
        Parameter 3 - Mapping Value (Eg. CC# + optional CC Value)


        Mapping Type Definitions:

            Type 1 Internal:
                Button is can be set to toggle between profiles.
                Toggle can:
                    - Temporarily switch to another profile while button is being held and revert to previous if released
                    - Button can switch to another profile when button is pressed
                    - Button can switch to next/previous profile (in the group [tag]) on press.
                    - Button can enable browse mode and knob can be used to toggle between profiles for quick access

            Type 2 Virtual HID:
                - Knob is by default assigned to some computer (Served by RobotJS) readable value 
                - Button can be assigned to any computer (Served by RobotJS) readable keystroke value
                - Button on press/hold can assign new value to Knob

            Type 3 MIDI:
                - Knob or Button can have set different MIDI Channel
                - Knob can hav assigned CC# and CC Value is updated on rotation CW or CCW
                - Button Can have assigned CC# and fixed CC Value
                - Button can modify CC# value of Knob (press/hold)

        Mapping Values:

            If MIDI is assigned to key:
                [type][trigger][channel][cc_num][cc_val]
                1016001255 - Midi(1), Trigger(0) - Press, Midi Channel 16, CC#01, CC 255

            If Virtual HID is assigned to key
                [type][string] (?)
                0STRING - USB(0), Some string (?)
                Some computer readible value is being passed that is further interpreted by RobotJS

            If Internal is assigned to key
                [type][trigger][uid]
                214567 - Internal(2), Trigger(1) - Hold, UID4567


        Trigger:
            Press -> Permanent Change
            Hold -> Temporary Change, on release revert to previous

        Above is highly theoritical and not ultimate solution, this should be discussed on best approach.
        Are there good common practives or case study that we could model mapping model and way the values are being stored 
        and then interpreted by firmware and softwae reliably?

    */