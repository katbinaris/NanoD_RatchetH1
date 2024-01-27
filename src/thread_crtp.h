#pragma once

#include<Arduino.h>

/*
 CRTP (Curiously Recurring Template Pattern) for new thread handling.
 It help to conserve a valuable program and votile memory when many tasks are being defined
 Source 1: https://fluentcpp.com/2017/05/16/what-the-crtp-brings-to-code/
 Source 2: https://fjrg76.wordpress.com/2021/05/18/my-view-on-static-polymorphism-through -the-crtp-pattern/
*/

template <class T>
class Thread {
    public:
        Thread(const char* name, uint32_t stackDepth, UBaseType_t priority, const BaseType_t coreId = tskNO_AFFINITY) : 
                name { name },
                stackDepth {stackDepth},
                priority { priority },
                coreId { coreId }
        {}
        virtual ~Thread() {};

        TaskHandle_t getHandle() {
            return taskHandle;
        }

        void begin() {
            BaseType_t result = xTaskCreatePinnedToCore(taskFunction, name, stackDepth, this, priority, &taskHandle, coreId);
            assert("Failed to create thread" && result == pdPASS);
        }

    private:
        static void taskFunction(void* params) {
            T* t = static_cast<T*>(params);
            t->run();
        }

        const char* name;
        uint32_t stackDepth;
        UBaseType_t priority;
        TaskHandle_t taskHandle;
        const BaseType_t coreId;
};