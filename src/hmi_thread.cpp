#include "hmi_thread.h"

#define BRIGHTNESS 10

using namespace ace_button;


AceButton button(PIN_BTN_A);

void handleEvent(AceButton*, uint8_t, uint8_t);

bool gReverseDirection = false;
CRGB leds[NANO_LED_A_NUM];

// Light Effects
void halvesPointer(int indicator, const struct CRGB& pointerCol, const struct CRGB& preCol, const struct CRGB& postCol);
void strobe(int delay, const struct CRGB& strobeCol);
void breathing(int speed, const struct CRGB& fadeCol);


// Hmi thread controls LED via FastLed and buttons via AceButton
// This thread send and receive data

HmiThread::HmiThread(const uint8_t task_core ) : Thread("HMI", 2048, 1, task_core) {}

HmiThread::~HmiThread() {}

void HmiThread::run() {
    

  FastLED.addLeds<LED_CHIPSET, PIN_LED_A, LED_COL_ORDER>(leds, NANO_LED_A_NUM);
  FastLED.setBrightness( BRIGHTNESS );
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  button.setEventHandler(handleEvent);

    while (1)
    {
        breathing(60, CRGB::Green);
        button.check();
    }
    
    
}

void handleEvent(AceButton*, uint8_t eventType, uint8_t) {
    switch (eventType) {
        case AceButton::kEventPressed:
        halvesPointer(37, CRGB::White, CRGB::Orange, CRGB::Teal);
        break;
        case AceButton::kEventReleased:
        halvesPointer(35, CRGB::White, CRGB::Orange, CRGB::Green);
        break;
    }
}

// Standard Pointer with two halves
void halvesPointer(int indicator, const struct CRGB& pointerCol, const struct CRGB& preCol, const struct CRGB& postCol){ 

    
    for (int i = 0; i < NANO_LED_A_NUM; ++i) {
        if(i > indicator) {
            leds[i] = postCol;
        }
        if(i < indicator) {
            leds[i] = preCol;
        }
    }
        leds[indicator] = pointerCol;
        FastLED.show();
        delay(1000 / 12);
}

// Quick Strobe Single Color
void strobe(int pps, const struct CRGB& strobeCol){

    fill_solid(leds, 60, strobeCol);
    FastLED.show();
    delay(1000/pps);
    
    fill_solid(leds, 60, CRGB::Black);
    FastLED.show();
    delay(1000/pps);

}


// Breathing to Black
int brightness = 0;
int fadeAmount = 5;

void breathing(int fps, const struct CRGB& fadeCol){
    
   for(int i = 0; i < NANO_LED_A_NUM; i++ )
   {
   leds[i] = fadeCol;  // Set Color HERE!!!
   leds[i].fadeToBlackBy(brightness);
  }
  FastLED.show();
  brightness = brightness + fadeAmount;
  // reverse the direction of the fading at the ends of the fade: 
  if(brightness == 0 || brightness == 255)
  {
    fadeAmount = -fadeAmount ;
    
  }    
  delay(1000/fps); 
}

