#include <Arduino.h>

#include "nanofoc_d.h"
#include "./foc_thread.h"
#include "./hmi_thread.h"
#include "./lcd_thread.h"
#include "./com_thread.h"
#include "./DeviceSettings.h"
#include <esp_task_wdt.h>

FocThread foc_thread(1);
HmiThread hmi_thread(0);
LcdThread lcd_thread(0);
ComThread com_thread(0);

void setup() {
  Serial.begin(DEFAULT_SERIAL_SPEED);
  Serial.println("Welcome to Nano_D++!");
  Serial.print("Firmware version: ");
  Serial.println(NANO_FIRMWARE_VERSION);
  // before we begin, load our global settings...
  DeviceSettings& settings = DeviceSettings::getInstance();
  settings.fromSPIFFS(); // attempt to load settings from SPIFFS

  com_thread.begin();
  hmi_thread.begin(settings.ledMaxBrightness);
  foc_thread.begin();
  lcd_thread.begin();
  vTaskDelete(NULL);
  
}

void loop() {
  // main loop not used...
  vTaskDelay(1000);
}