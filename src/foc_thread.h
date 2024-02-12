#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include "haptic.h"
#include "hmi_thread.h"


class FocThread : public Thread<FocThread> {
    friend class Thread<FocThread>; //Allow Base Thread to invoke protected run()

    public:
        FocThread(const uint8_t task_core, HmiThread& hmi_thread);
        ~FocThread();
        
        
        
    
    protected:
        void run();

    private:
HmiThread& hmi_thread_;
    QueueHandle_t nano_queue_;
    };