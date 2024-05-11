#include <Arduino.h>

#include "nanofoc_d.h"
#include "./foc_thread.h"
#include "./hmi_thread.h"
#include "./lcd_thread.h"
#include "./com_thread.h"
#include "./DeviceSettings.h"
#include <esp_task_wdt.h>
#include "SPIFFS.h"
#include <Adafruit_TinyUSB.h>

FocThread foc_thread(1);
HmiThread hmi_thread(0);
LcdThread lcd_thread(0);
ComThread com_thread(0);



void setup() {

  // initialize USB
  TinyUSBDevice.begin();
  hmi_thread.init_usb();
  TinyUSBDevice.setID(0x239A, 0x8010); // TODO move to #define
  TinyUSBDevice.setProductDescriptor("Nano_D++ (Beta)"); // TODO move to #define
  TinyUSBDevice.setManufacturerDescriptor("Binaris Circuitry");
  TinyUSBDevice.setSerialDescriptor("Nano_D");
  //TinyUSBDevice.attach();
  Serial.begin(DEFAULT_SERIAL_SPEED);
  // while( !TinyUSBDevice.mounted() ) delay(1);

  delay(100);
  Serial.println("Welcome to Nano_D++!");
  Serial.print("Firmware version: ");
  Serial.println(NANO_FIRMWARE_VERSION);
  Serial.println("Initializing...");
  // before we begin, load our global settings...
  DeviceSettings& settings = DeviceSettings::getInstance();
  settings.init();
  settings.fromSPIFFS(); // attempt to load settings from SPIFFS

  // initialize PD power
  hmi_thread.init_pd();

  // then load the profiles
  HapticProfileManager& profileManager = HapticProfileManager::getInstance();
  profileManager.fromSPIFFS(); // attempt to load profiles from SPIFFS

  // load motor calibration from Preferences
  MotorCalibration cal = settings.loadCalibration();
  foc_thread.setCalibration(cal);
  
  // load current profile from Preferences
  String current_profile = settings.loadCurrentProfile();
  profileManager.setCurrentProfile(current_profile);

  // init threads
  hmi_thread.init(profileManager.getCurrentProfile()->led_config, profileManager.getCurrentProfile()->hmi_config);
  if (profileManager.getCurrentProfile()->hmi_config.knob.num > 0)
    foc_thread.init(profileManager.getCurrentProfile()->hmi_config.knob.values[0].haptic);

  // start threads
  Serial.println("Starting threads...");
  Serial.flush();
  vTaskDelay(100 / portTICK_PERIOD_MS);
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
