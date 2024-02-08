#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include <HardwareSerial.h>
#include <MIDI.h>


class MidiThread : public Thread<MidiThread> {
    friend class Thread<MidiThread>; //Allow Base Thread to invoke protected run()
    public:
        MidiThread(const uint8_t task_core);
        ~MidiThread();

    protected:
        void run();
};