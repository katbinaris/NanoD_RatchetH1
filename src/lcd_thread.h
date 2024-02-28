#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "thread_crtp.h"
#include "ui.h"

class LcdThread : public Thread<LcdThread> {
    friend class Thread<LcdThread>; //Allow Base Thread to invoke protected run()

    public:
        LcdThread(const uint8_t task_core);
        ~LcdThread();
        
    
    protected:
        void run();

    private:
        
};