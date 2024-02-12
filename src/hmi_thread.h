#pragma once

#include <Arduino.h>
#include <AceButton.h>
#include <FastLED.h>
#include "thread_crtp.h"


class HmiThread : public Thread<HmiThread> {
    friend class Thread<HmiThread>; //Allow Base Thread to invoke protected run()

    public:
        HmiThread(const uint8_t task_core);
        ~HmiThread();
       
    
    protected:
        void run();

    private:
        
};