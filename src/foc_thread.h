#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include "haptic.h"
#include "nanofoc_d.h"
#include "DeviceSettings.h"
#include "audio/audio_api.h"


class FocThread : public Thread<FocThread> {
    friend class Thread<FocThread>; //Allow Base Thread to invoke protected run()
    friend class HapticInterface;

    public:
        FocThread(const uint8_t task_core);
        ~FocThread();

        void init(DetentProfile& initialConfig);

        void put_motor_command(String* msg);
        void put_haptic_config(DetentProfile& profile);
        bool get_angle_event(AngleEvt* evt);
    

        float get_motor_angle();
        
        uint16_t pass_cur_pos();
        uint16_t pass_start_pos();
        uint16_t pass_end_pos();
        uint16_t pass_last_pos();
        bool pass_at_limit();

        void setCalibration(MotorCalibration& cal);

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
