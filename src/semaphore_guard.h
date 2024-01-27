#pragma once

#include <Arduino.h>

class SemaphoreGuard {
    public:
        SemaphoreGuard(SemaphoreHandle_t handle) : handle_{handle} {
            xSemaphoreTake(handle_, portMAX_DELAY);
        }
        ~SemaphoreGuard() {
            xSemaphoreGive(handle_);
        }
        SemaphoreGuard(SemaphoreGuard const&)=delete;
        SemaphoreGuard& operator=(SemaphoreGuard const&)=delete;

    private:
        SemaphoreHandle_t handle_;
};