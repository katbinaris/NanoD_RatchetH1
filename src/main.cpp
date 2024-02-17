#include <Arduino.h>

#include "foc_thread.h"
#include "hmi_thread.h"
#include "lcd_thread.h"
#include "com_thread.h"
#include <esp_task_wdt.h>

FocThread foc_thread(1);
HmiThread hmi_thread(0);
LcdThread lcd_thread(0);
ComThread com_thread(0);

//unsigned long ts = 0;

void setup() {
  // Serial.begin(115200); // doesn't matter, its USB Serial
  // delay(5000); // wait 5s, TODO remove me!
  // Serial.println("Starting threads");
 com_thread.begin();
 //foc_thread.begin();

  vTaskDelete(NULL); // this causes kernel panic
  //ts = millis();
}

void loop() {
  // Main loop left empty
  // if (millis()-ts>1000) {
  //   ts = millis();
  //   Serial.println("Looping");
  // }
  vTaskDelay(1000);
}