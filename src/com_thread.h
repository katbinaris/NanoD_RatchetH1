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
    STRING_MESSAGE_MOTOR
};

class StringMessage {
    public:
        StringMessage(String* message = nullptr, StringMessageType type = StringMessageType::STRING_MESSAGE_DEBUG) : message(message),  type(type) {}
        String* message;// = nullptr;
        StringMessageType type = STRING_MESSAGE_DEBUG;
};



class ComThread : public Thread<ComThread> {
    friend class Thread<ComThread>; //Allow Base Thread to invoke protected run()
    public:
        ComThread(const uint8_t task_core);
        ~ComThread();

        void setCurrentProfile(String name);
        void put_string_message(StringMessage& msg);
        bool isProfileNameOk(String& name);

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

        void sendError(String& error, String* msg = nullptr);
        void sendError(String& error, String& msg);
        void sendError(const char* error, String& msg);
        void sendError(const char* error, const char* msg = nullptr);

        QueueHandle_t _q_strings_in;
        unsigned long ts_last_activity;
};


extern ComThread com_thread;