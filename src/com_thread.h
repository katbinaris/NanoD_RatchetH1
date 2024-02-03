#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include <HardwareSerial.h>
#include <MIDI.h>


class ComThread : public Thread<ComThread> {
    friend class Thread<ComThread>; //Allow Base Thread to invoke protected run()

    public:
        ComThread(const uint8_t task_core);
        ~ComThread();
        
    
    protected:
        void run();

    private:
    
    
    
        
};