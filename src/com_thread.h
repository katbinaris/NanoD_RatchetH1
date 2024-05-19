#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include <HardwareSerial.h>
#include <MIDI.h>
#include <ArduinoJSON.h>
#include "HapticProfileManager.h"


enum StringMessageType {
    STRING_MESSAGE_DEBUG,
    STRING_MESSAGE_ERROR,
    STRING_MESSAGE_MOTOR,
    STRING_MESSAGE_PROFILE,
    STRING_MESSAGE_NEXT_PROFILE,
    STRING_MESSAGE_PREV_PROFILE
};

class StringMessage {
    public:
        StringMessage(String* message = nullptr, StringMessageType type = StringMessageType::STRING_MESSAGE_DEBUG) : message(message),  type(type) {};
        String* message;// = nullptr;
        StringMessageType type;
};



class ComThread : public Thread<ComThread> {
    friend class Thread<ComThread>; //Allow Base Thread to invoke protected run()
    public:
        ComThread(const uint8_t task_core);
        ~ComThread();

        void setCurrentProfile(String name);
        void put_string_message(const StringMessage& msg);
        bool isProfileNameOk(String& name);
        
        bool global_sleep_flag = false;
        uint32_t global_idle_timeout = 5000;

    protected:
        void run();
        void handleProfileCommand(JsonVariant profile, JsonVariant updates);
        void handleSettingsCommand(JsonVariant s);
        void handleProfilesCommand(JsonVariant p);
        void handleMessages();
        void handleEvents();

        void dispatchLedConfig();
        void dispatchHapticConfig();
        void dispatchHmiConfig();
        void dispatchSettings();
        void dispatchAudioConfig();
        void dispatchLcdConfig();

        String generateDescription(HapticProfile& curr);

        void sendError(String& error, String* msg = nullptr);
        void sendError(String& error, String& msg);
        void sendError(const char* error, String& msg);
        void sendError(const char* error, const char* msg = nullptr);

        QueueHandle_t _q_strings_in;
        unsigned long ts_last_activity;
};


extern ComThread com_thread;