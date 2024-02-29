#pragma once

#include <Arduino.h>
#include <AceButton.h>
#include <FastLED.h>
#include "thread_crtp.h"
#include "nanofoc_d.h"


class HmiThread : public Thread<HmiThread> {
    friend class Thread<HmiThread>; //Allow Base Thread to invoke protected run()

    public:
        HmiThread(const uint8_t task_core);
        ~HmiThread();
       
        void begin(const uint8_t ledMaxBrightness);
    
        // button
        void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState);

        // Light Effects
        void halvesPointer(int indicator, const struct CRGB& pointerCol, const struct CRGB& preCol, const struct CRGB& postCol);
        void strobe(int pps, const struct CRGB& strobeCol);
        void breathing(int fps, const struct CRGB& fadeCol);

    protected:
        void run();
        uint8_t ledMaxBrightness = DEFAULT_LED_MAX_BRIGHTNESS;
        
        ace_button::AceButton button;
        bool gReverseDirection = false;
        CRGB leds[NANO_LED_A_NUM];
        CRGB ledsp[4];
        // Breathing to Black
        int brightness = 0;
        int fadeAmount = 5;

};