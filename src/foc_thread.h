#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include "haptic.h"
#include "nanofoc_d.h"


class FocThread : public Thread<FocThread> {
    friend class Thread<FocThread>; //Allow Base Thread to invoke protected run()

    public:
        FocThread(const uint8_t task_core);
        ~FocThread();

        void put_message(String* msg);
        String* get_message();
        void put_haptic_config(hapticConfig* profile);

    protected:
        void run();
        void handleMessage();
        void handleHapticConfig();

    private:
        QueueHandle_t _q_in;
        QueueHandle_t _q_out;
        QueueHandle_t _q_haptic;
};

extern FocThread foc_thread;
