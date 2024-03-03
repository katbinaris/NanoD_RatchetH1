
#pragma once

#include <inttypes.h>

/**
 * This file contains the API for the LED module. 
 * The fields defined here are needed to exchange configuration data
 * between the comms thread and the LED thread.
 */


typedef struct {


    // Light Settings
    bool led_enable = true; 

    uint8_t led_brightness = 100; // 0-255

    // If True FastLED is enabled
    uint8_t led_mode = 0;


    /*
        Led Mode toggles between 4 pre-defined led feedback variants
        0 - Solid Color (Unreactive)
        1 - Pulse (Breathing Animation which uses Delay in for loop)
        2 - Half Follow Pointer (Active half is filled with Primary color + pointer [current_pos])
        3 - Full Follow Pointer (Active half is filled with Primary Color + pointer [current_pos ] + Inactive Half is filled with Secondary Color)

        Note:
        Led Mode can be dynamic in operation eg. Upon holding key [A...D] led_mode can be modified to 1 (Pulse), 
        Upon releasing it would return to default value stored in profile config.
    */


    // LED Ring array: Pointer, Primary, Scondary always store only one HEX value
    uint32_t pointer_col = 0xFFFFFF;
    uint32_t primary_col = 0x008000;
    uint32_t secondary_col = 0xFFA500;

    uint32_t button_A_col_idle = 0xFCBA03; 
    uint32_t button_B_col_idle = 0xFCBA03; 
    uint32_t button_C_col_idle = 0xFCBA03; 
    uint32_t button_D_col_idle = 0xFCBA03; 

    uint32_t button_A_col_press = 0xFC03EB;
    uint32_t button_B_col_press = 0xFC03EB;
    uint32_t button_C_col_press = 0xFC03EB;
    uint32_t button_D_col_press = 0xFC03EB;


    /* 
        Note: 
        Unsure how these properties can be stored more efficiently without having too many variables.
        Perhaps idle and press color of each button could be stored in one value as array?
        Should these values be 3byte each? Is it maybe better to store color values for buttons in Button Mapping?

    */

    // Gui Settings

    /*
        This entire section can be ignored for now. In future GUI settings can contain following parameters:
        uint8_t primaryGuiFont = 1 - Device would have pre-defined fonts, user is not able to update own font.
        uint8_t secondaryGuiFont = 0 - Device would have pre-defined fonts, user is not able to update own font.
        uint8_t styleGui = 1 - Device can have pre-defined GUI styles/templates, user can choose between 2-3 variants per profile. LCD_THREAD handles its interpretation
        bool devGuiOverlay = - Optional advanced option that could show current FPS or highlight/outilne currently refreshed area of the buffer (?)
    */

} ledConfig;
