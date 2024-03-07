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

        void init(hapticConfig& initialConfig);

        void put_motor_command(String* msg);
        void put_haptic_config(hapticConfig& profile);
        bool get_angle_event(AngleEvt* evt);

        float get_motor_angle();

    protected:
        void run();
        void handleMessage();
        void handleHapticConfig();

        float angleEventMinAngle = 0.017453292519943f; // 1° in radians
        uint32_t angleEventMinMicroseconds = 10000; // 100Hz

    private:
        QueueHandle_t _q_motor_in;
        QueueHandle_t _q_haptic_in;
        QueueHandle_t _q_angleevt_out;
};

extern FocThread foc_thread;
