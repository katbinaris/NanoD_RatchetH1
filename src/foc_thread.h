#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include "haptic.h"


class FocThread : public Thread<FocThread> {
    friend class Thread<FocThread>; //Allow Base Thread to invoke protected run()

    public:
        FocThread(const uint8_t task_core);
        ~FocThread();
        
    
    protected:
        void run();

    private:
    QueueHandle_t queue_;
    };