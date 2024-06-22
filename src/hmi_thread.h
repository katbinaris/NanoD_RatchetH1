#pragma once

#include <Arduino.h>
#include <AceButton.h>
#include <Adafruit_NeoPixel.h>
#include "thread_crtp.h"
#include "nanofoc_d.h"
#include "./led_api.h"
#include "./hmi_api.h"
#include "./DeviceSettings.h"

using namespace ace_button;


typedef enum {
    POWER_5V_USB = 0,
    POWER_5V_PD = 1,
    POWER_9V_PD = 2
} PowerType;


class HmiThreadButtonHandler : public IEventHandler  {
public:
    HmiThreadButtonHandler(uint8_t _index = 0);
    uint8_t index;
    void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) override;
};



class HmiThread : public Thread<HmiThread> {
    friend class Thread<HmiThread>; //Allow Base Thread to invoke protected run()
    friend class HmiThreadButtonHandler;
    friend class ComThread;


    public:
        HmiThread(const uint8_t task_core);
        ~HmiThread();
       
        bool init_usb();
        PowerType init_pd();
        uint32_t read_pdstatus();
        void init(ledConfig& initial_led_config, hmiConfig& initial_hmi_config);
    
        // queues
        void put_led_config(ledConfig& new_config);
        bool get_key_event(KeyEvt* keyEvt);
        void put_hmi_config(hmiConfig& new_config);
        void put_settings(HmiDeviceSettings& new_settings);

        // Light Effects
        void halvesPointer(int indicator, int startpos, int endpos, int orientation, uint32_t pointerCol, uint32_t preCol, uint32_t postCol);
        void IdleLeds(int fps, uint32_t idleColStart, uint32_t idleColMid, uint32_t idleColEnd);

        void handleSysex(byte* array, unsigned size);

    protected:
        void run();
        
        QueueHandle_t _q_config_in;
        QueueHandle_t _q_hmi_config_in;
        QueueHandle_t _q_settings_in;
        QueueHandle_t _q_keyevt_out;

        // internal queue handler
        void handleConfig();
        void handleSettings();


        // LEDs
        uint8_t led_max_brightness = 100;
        ledConfig led_config;
        uint32_t leds[NANO_LED_A_NUM];
        uint32_t ledsp[NANO_LED_B_NUM];
        unsigned long lastCheck = 0;
        uint16_t last_pos = -1;
        bool isIdle = false;
        void updateKeyLeds();
        void updateLeds();

        // buttons
        hmiConfig hmi_config;
        HmiThreadButtonHandler button_handler[4];
        ace_button::AceButton* buttons[4];
        uint8_t num_key_codes = 0;
        uint8_t last_num_key_codes = 0;
        uint8_t current_key_codes[6] = { 0 };
        uint8_t keyState = 0;
        uint8_t current_mouse_buttons = 0;
        uint8_t last_mouse_buttons = 0;
        uint8_t current_pad_buttons = 0;
        uint8_t last_pad_buttons = 0;

        // button handler
        void handleKeyAction(keyAction& action, uint8_t eventType);
        void handleHid();

        // knob
        float lastValue;
        float currentValue;
        void updateValue();

        // midi config
        void handleMidi();
        midiSettings midiUsbSettings;
        midiSettings midi2Settings;
        uint8_t midi_sysex_id = 0x00;

        // animations
        bool gReverseDirection = false;
        int brightness = 0;
        int fadeAmount = 10;

};

extern HmiThread hmi_thread;
