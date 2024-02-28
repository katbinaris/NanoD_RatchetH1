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

void setup() {
  com_thread.begin();
  hmi_thread.begin();
  foc_thread.begin();
  lcd_thread.begin();
  vTaskDelete(NULL);
  
}

void loop() {
  // main loop not used...
  vTaskDelay(1000);
}