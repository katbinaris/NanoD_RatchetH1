#pragma once

#ifndef NANOFOCD_H

#define ONCETOP 1

// Motor driver
#define PIN_EN_U 33
#define PIN_EN_V 48
#define PIN_EN_W 36
#define PIN_IN_U 34
#define PIN_IN_V 35
#define PIN_IN_W 37

// Magnetic encoder (MA)
#define PIN_MAG_DO 21
#define PIN_MAG_CLK 18
#define PIN_MAG_CS 17

// Smart LED (WS2811)
#define PIN_LED_A 38
#define PIN_LED_B 42
#define NANO_LED_A_NUM 60
#define NANO_LED_B_NUM 4
#define LED_COL_ORDER GRB
#define LED_CHIPSET WS2811

// Keypad
#define PIN_BTN_A 41
#define PIN_BTN_B 40
#define PIN_BTN_C 45
#define PIN_BTN_D 46

// Transducer
#define PIN_I2S_DOUT 9
#define PIN_I2S_BCLK 10
#define PIN_I2S_LRC 11

// Display control
#define PIN_LCD_RST 2
#define PIN_LCD_SCK 3
#define PIN_LCD_DATA 4
#define PIN_LCD_BLK 5
#define PIN_LCD_CS 6
#define PIN_LCD_CMD 7

// Misc
#define NANO_FS 0
#define NANO_DISPLAY 0
#define NANO_AUDIO 0
#define NANO_LED 1
#define NANO_MOTOR 1
#define NANO_KEY 1
#define NANO_MODE 1
#define NANO_MIDI 0
#define NANO_PWM_FREQ 0
#define NANO_LOOP_FREQ 0
#define NANO_SPI0_FREQ 0
#define NANO_SPI1_FREQ 0

#endif