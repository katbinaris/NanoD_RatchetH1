#include "hmi_thread.h"

using namespace ace_button;


// Hmi thread controls LED via FastLed and buttons via AceButton

HmiThread::HmiThread(const uint8_t task_core ) : Thread("HMI", 2048, 1, task_core), keyA(PIN_BTN_A), keyB(PIN_BTN_B), keyC(PIN_BTN_C), keyD(PIN_BTN_D) {
    _q_config_in = xQueueCreate(2, sizeof( ledConfig ));
    _q_keyevt_out = xQueueCreate(5, sizeof( KeyEvt ));
}

HmiThread::~HmiThread() {}

void HmiThread::init(ledConfig& initialConfig) {
    led_config = initialConfig;
    FastLED.setBrightness(led_config.led_brightness);
};


void HmiThread::put_led_config(ledConfig& newConfig) {
    xQueueSend(_q_config_in, &newConfig, (TickType_t)0);
};



void HmiThread::handleConfig() {
    ledConfig newConfig;
    if (xQueueReceive(_q_config_in, &newConfig, (TickType_t)0)) {
        led_config = newConfig;
        FastLED.setBrightness(led_config.led_brightness);
    }
};



bool HmiThread::get_key_event(KeyEvt* keyEvt){
    return xQueueReceive(_q_keyevt_out, keyEvt, (TickType_t)0);
};


void HmiThread::run() {
    FastLED.addLeds<LED_CHIPSET, PIN_LED_A, LED_COL_ORDER>(leds, NANO_LED_A_NUM);
    FastLED.addLeds<LED_CHIPSET, PIN_LED_B, LED_COL_ORDER>(ledsp, NANO_LED_B_NUM);
    FastLED.setBrightness( DEFAULT_LED_MAX_BRIGHTNESS );
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);
    pinMode(PIN_BTN_D, INPUT_PULLUP);
    keyA.getButtonConfig()->setIEventHandler(this);
    keyB.getButtonConfig()->setIEventHandler(this);
    keyC.getButtonConfig()->setIEventHandler(this);
    keyD.getButtonConfig()->setIEventHandler(this);
    ledsp[0] = CRGB(led_config.button_A_col_idle);
    ledsp[1] = CRGB(led_config.button_B_col_idle);
    ledsp[2] = CRGB(led_config.button_C_col_idle);
    ledsp[3] = CRGB(led_config.button_D_col_idle);

    while (1) {
        keyA.check();
        keyB.check();
        keyC.check();
        keyD.check();
        handleConfig();
        halvesPointer(30, CRGB(led_config.pointer_col), CRGB(led_config.primary_col), CRGB(led_config.secondary_col));
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    
};




void HmiThread::handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
    int8_t keyNum = -1;
    if (button == &keyA) {
        keyNum = 0;
    } else if (button == &keyB) {
        keyNum = 1;
    } else if (button == &keyC) {
        keyNum = 2;
    } else if (button == &keyD) {
        keyNum = 3;
    }
    switch (eventType) {
        case AceButton::kEventPressed:
            switch (keyNum) {
                case 0:
                    ledsp[0] = CRGB(led_config.button_A_col_press);
                    keyState |= 1;
                break;
                case 1:
                    ledsp[1] = CRGB(led_config.button_B_col_press);
                    keyState |= 2;
                break;
                case 2:
                    ledsp[2] = CRGB(led_config.button_C_col_press);
                    keyState |= 4;
                break;
                case 3:
                    ledsp[3] = CRGB(led_config.button_D_col_press);
                    keyState |= 8;
                break;
            }
        break;
        case AceButton::kEventReleased:
            switch (keyNum) {
                case 0:
                    ledsp[0] = CRGB(led_config.button_A_col_idle);
                    keyState &= ~1;
                break;
                case 1:
                    ledsp[1] = CRGB(led_config.button_B_col_idle);
                    keyState &= ~2;
                break;
                case 2:
                    ledsp[2] = CRGB(led_config.button_C_col_idle);
                    keyState &= ~4;
                break;
                case 3:
                    ledsp[3] = CRGB(led_config.button_D_col_idle);
                    keyState &= ~8;
                break;
            }
        break;
    }
    if (keyNum >= 0) {
        KeyEvt keyEvt = { .type=eventType, .keyNum=(uint8_t)keyNum, .keyState=keyState };
        xQueueSend(_q_keyevt_out, &keyEvt, (TickType_t)0);
    }
};




// Standard Pointer with two halves
void HmiThread::halvesPointer(int indicator, const struct CRGB& pointerCol, const struct CRGB& preCol, const struct CRGB& postCol){ 
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
    vTaskDelay(1000 / 12 / portTICK_PERIOD_MS);
};



// Quick Strobe Single Color
void HmiThread::strobe(int pps, const struct CRGB& strobeCol){
    fill_solid(leds, 60, strobeCol);
    FastLED.show();
    vTaskDelay(1000/pps / portTICK_PERIOD_MS);
    
    fill_solid(leds, 60, CRGB::Black);
    FastLED.show();
    vTaskDelay(1000/pps / portTICK_PERIOD_MS);
};




void HmiThread::breathing(int fps, const struct CRGB& fadeCol){
    for(int i = 0; i < NANO_LED_A_NUM; i++ ) {
        leds[i] = fadeCol;  // Set Color HERE!!!
        leds[i].fadeToBlackBy(brightness);
    }
    FastLED.show();
    brightness = brightness + fadeAmount;
    brightness = constrain(brightness, 0, led_config.led_brightness);
    // reverse the direction of the fading at the ends of the fade: 
    if(brightness <= 0 || brightness >= led_config.led_brightness) {
        fadeAmount = -fadeAmount ;
    }
    vTaskDelay(1000/fps / portTICK_PERIOD_MS); 
};

