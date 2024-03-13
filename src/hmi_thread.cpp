#include "hmi_thread.h"
#include "com_thread.h"
#include "foc_thread.h"
#include <Adafruit_TinyUSB.h>
#include "MIDI.h"

using namespace ace_button;


Adafruit_USBD_MIDI usb_midi(1);

MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI_IO);

enum
{
  RID_KEYBOARD = 1,
  RID_MOUSE = 2
};


uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(RID_KEYBOARD) ),
  TUD_HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(RID_MOUSE) )
};

// USB HID object
Adafruit_USBD_HID usb_hid;





// Hmi thread controls LED via FastLed and buttons via AceButton

HmiThread::HmiThread(const uint8_t task_core ) : Thread("HMI", 2048, 1, task_core), keyA(PIN_BTN_A), keyB(PIN_BTN_B), keyC(PIN_BTN_C), keyD(PIN_BTN_D) {
    _q_config_in = xQueueCreate(2, sizeof( ledConfig ));
    _q_keyevt_out = xQueueCreate(5, sizeof( KeyEvt ));
}

HmiThread::~HmiThread() {}


// init_usb() must be called before the thread is started
void HmiThread::init_usb() {
  usb_midi.setStringDescriptor("Nano_D MIDI");
  //usb_midi.setCableName(1, "Port1");
  //usb_midi.setCableName(2, "Port THRU");
  usb_midi.begin();

  usb_hid.setBootProtocol(HID_ITF_PROTOCOL_NONE);
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Nano_D HID");
  usb_hid.begin();
};


// init must be called before the thread is started
void HmiThread::init(ledConfig& initial_led_config, hmiConfig& initial_hmi_config) {
    led_config = initial_led_config;
    hmi_config = initial_hmi_config;
    FastLED.setBrightness(led_config.led_brightness);
};


void HmiThread::put_led_config(ledConfig& new_config) {
    xQueueSend(_q_config_in, &new_config, (TickType_t)0);
};



void HmiThread::handleConfig() {
    ledConfig newConfig;
    if (xQueueReceive(_q_config_in, &newConfig, (TickType_t)0)) {
        led_config = newConfig;
        FastLED.setBrightness(led_config.led_brightness);
        updateKeyLeds();
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
    keyA.getButtonConfig()->setClickDelay(50);
    keyB.getButtonConfig()->setClickDelay(50);
    keyC.getButtonConfig()->setClickDelay(50);
    keyD.getButtonConfig()->setClickDelay(50);
    keyA.getButtonConfig()->clearFeature(ButtonConfig::kFeatureDoubleClick);
    keyB.getButtonConfig()->clearFeature(ButtonConfig::kFeatureDoubleClick);
    keyC.getButtonConfig()->clearFeature(ButtonConfig::kFeatureDoubleClick);
    keyD.getButtonConfig()->clearFeature(ButtonConfig::kFeatureDoubleClick);
    ledsp[0] = CRGB(led_config.button_A_col_idle);
    ledsp[1] = CRGB(led_config.button_B_col_idle);
    ledsp[2] = CRGB(led_config.button_C_col_idle);
    ledsp[3] = CRGB(led_config.button_D_col_idle);

    unsigned long total = 0;
    unsigned long updates = 0;
    unsigned long ts = micros();

    while (1) {
        keyA.check();
        keyB.check();
        keyC.check();
        keyD.check();
        handleConfig();
        updateLeds();
        unsigned long us = micros();
        FastLED.show();
        unsigned long now = micros();
        us = now - us;
        updates++;
        total += us;
        if (now - ts > 10000000) {
            float fps = 1000000.0 * updates / total;
            float ups = 1000000.0 * updates / (now - ts);
            StringMessage msg(new String("LED rate: " + String(ups) + " updates/s, "+ String(fps) +" frames/s raw (= " + String(fps * 64) + " pix/s = " + String(fps * 64 * 3 * 8 / 1000000) + "Mbps"));
            com_thread.put_string_message(msg);
            ts = now;
            total = 0;
            updates = 0;
        }
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
    if (keyNum >= 0) {
        switch (eventType) {
            case AceButton::kEventPressed:
                keyState |= (1<<keyNum);
                for (int i=0; i<hmi_config.keys[keyNum].num_pressed_actions; i++) {
                    handleKeyAction(hmi_config.keys[keyNum].pressed[i]);
                }
            break;
            case AceButton::kEventReleased:
                keyState &= ~(1<<keyNum);
            break;
        }
        KeyEvt keyEvt = { .type=eventType, .keyNum=(uint8_t)keyNum, .keyState=keyState };
        xQueueSend(_q_keyevt_out, &keyEvt, (TickType_t)0);
        updateKeyLeds();
    }
};





void HmiThread::handleKeyAction(keyAction& action) {
    switch (action.type) {
        case keyActionType::KA_MIDI:
            //midi.sendControlChange(action.midi.midiChannel, action.midi.ccNum, action.midi.ccVal);
        break;
        case keyActionType::KA_KEY:
            // TODO implement
            for (int i=0;i<action.hid.num;i++) {
                usb_hid.keyboardPress(RID_KEYBOARD, action.hid.key_codes[i]);
            }
        break;
        case keyActionType::KA_MOUSE:
            // TODO implement
        break;
        case keyActionType::KA_GAMEPAD:
            // TODO implement
        break;
        case keyActionType::KA_PROFILE_CHANGE:
            // TODO implement
        break;
    }
};






void HmiThread::updateKeyLeds() {
    ledsp[0] = CRGB((keyState&0x1)?led_config.button_A_col_press:led_config.button_A_col_idle);
    ledsp[1] = CRGB((keyState&0x2)?led_config.button_B_col_press:led_config.button_B_col_idle);
    ledsp[2] = CRGB((keyState&0x4)?led_config.button_C_col_press:led_config.button_C_col_idle);
    ledsp[3] = CRGB((keyState&0x8)?led_config.button_D_col_press:led_config.button_D_col_idle);
};



void HmiThread::updateLeds() {
    // TODO implement switching animations
    uint8_t pointer = (foc_thread.get_motor_angle() / 6.283185307179586f) * 60; // TODO take device orientation into account
    halvesPointer(pointer, CRGB(led_config.pointer_col), CRGB(led_config.primary_col), CRGB(led_config.secondary_col));
}




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
    if (indicator < NANO_LED_A_NUM)
        leds[indicator] = pointerCol;
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

