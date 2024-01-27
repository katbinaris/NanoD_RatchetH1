#include <Arduino.h>
#include "lcd_thread.h"

// LCD Thread only receives data from COM Thread, sends nothing.


LcdThread::LcdThread(const uint8_t task_core) : Thread("LCD", 2048, 1, task_core) {}

LcdThread::~LcdThread() {}

void LcdThread::run() {
    //Serial.print("LCD running on core ");  TODO don't use Serial in threads
    //Serial.println(xPortGetCoreID());
    while (1) {
    //Serial.println("LCD Task: reporting duty");
    delay(200);
    }
}
