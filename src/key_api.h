

typedef struct {

    /* 

        Context:
        Mapping Buttons can play few roles:

        Case 1: Mapped Button can trigger another profile (eg. pres button -> go to next profile withing same group [tag] or go to specific profile [uid])
        Case 2: Mapped Button can toggle current/active CC # - knob sends values 0-127 the output message
        Case 3: Mapped Button can send specific CC # with specific value
        Case 4: Mapped Button contains mapping value that can be parsed by RobotJS (eg. Key 'B' or 'Arrow Up' or 'Ctrl')
        TODO: how mappings for buttons and knob should be stored as?

    */


    const char* button_A_map = "1016001255"; //  MODE: Midi(1), TRIGGER: Press(0), MIDI CH (16), CC#(01), CC(255)
    const char* button_B_map = "214567"; // MODE: Internal(2), TRIGGER: Hold(1), UID: (4567)
    const char* button_C_map = "0"; // No mapping -> Disables button and RGB assigned for that button
    const char* button_D_map = "0"; //etc.

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
} keyConfig;