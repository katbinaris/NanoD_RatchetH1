#include <Arduino.h>

#include "nanofoc_d.h"
#include "./foc_thread.h"
#include "./hmi_thread.h"
#include "./lcd_thread.h"
#include "./com_thread.h"
#include "./DeviceSettings.h"
#include <esp_task_wdt.h>
#include "SPIFFS.h"

FocThread foc_thread(1);
HmiThread hmi_thread(0);
LcdThread lcd_thread(0);
ComThread com_thread(0);

void setup() {
  Serial.begin(DEFAULT_SERIAL_SPEED);
  Serial.println("Welcome to Nano_D++!");
  Serial.print("Firmware version: ");
  Serial.println(NANO_FIRMWARE_VERSION);
  Serial.println("Initializing...");
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS mounted successfully");
  }
  else {
    Serial.println("ERROR: SPIFFS mount failed");
    // TODO this is kind of fatal...
  }

  // before we begin, load our global settings...
  DeviceSettings& settings = DeviceSettings::getInstance();
  settings.fromSPIFFS(); // attempt to load settings from SPIFFS
  // then load the profiles
  HapticProfileManager& profileManager = HapticProfileManager::getInstance();
  profileManager.fromSPIFFS(); // attempt to load profiles from SPIFFS

  // TODO load motor calibration from SPIFFS

  // init threads
  hmi_thread.init(profileManager.getCurrentProfile()->led_config);
  foc_thread.init(profileManager.getCurrentProfile()->haptic_config);

  // start threads
  Serial.println("Starting threads...");
  Serial.flush();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  com_thread.begin();
  hmi_thread.begin();
  foc_thread.begin();
  //lcd_thread.begin();
  vTaskDelete(NULL);
}



void loop() {
  // main loop not used...
  vTaskDelay(1000);
}

