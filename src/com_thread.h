#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include <HardwareSerial.h>
#include <MIDI.h>
#include <ArduinoJSON.h>
#include "HapticProfileManager.h"


class ComThread : public Thread<ComThread> {
    friend class Thread<ComThread>; //Allow Base Thread to invoke protected run()
    public:
        ComThread(const uint8_t task_core);
        ~ComThread();

        void setCurrentProfile(String name);
    protected:
        void run();
        void handleProfileCommand(JsonVariant p);
        void handleSettingsCommand(JsonVariant s);
        void handleProfilesCommand(JsonVariant p);

        void dispatchLedConfig();
        void dispatchHapticConfig();
};
