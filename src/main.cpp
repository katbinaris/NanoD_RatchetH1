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

#include <SparkFun_STUSB4500.h>

FocThread foc_thread(1);
HmiThread hmi_thread(0);
LcdThread lcd_thread(0);
ComThread com_thread(0);

STUSB4500 usb;


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

  // PD Sink Procedure 

    // Wire.begin(12, 13);
    // delay(5000);
    // if(!usb.begin()){
    //   Serial.println("Cannot connecto to STUSB4500");
    //   while(1);
    // }

    // Serial.println("Connected to STUSB4500");
    // delay(1000);

    // usb.setVoltage(1,5.0);
    // usb.setCurrent(1,3.0);
    // usb.setLowerVoltageLimit(1,15);
    // usb.setUpperVoltageLimit(1,15);

    // usb.setVoltage(2,9.0);
    // usb.setCurrent(2,3.0);
    // usb.setLowerVoltageLimit(2,10);
    // usb.setUpperVoltageLimit(2,10);

    // usb.setVoltage(3, 5.0);
    // usb.setCurrent(3,3.0);
    // usb.setLowerVoltageLimit(3,10);
    // usb.setUpperVoltageLimit(3,10);

    // usb.setUsbCommCapable(true);
    // usb.setPdoNumber(2);

    // usb.write();

    // usb.read();


  // Serial.println("New Parameters:\n");

  // /* Read the Power Data Objects (PDO) highest priority */
  // Serial.print("PDO Number: ");
  // Serial.println(usb.getPdoNumber());

  // /* Read settings for PDO1 */
  // Serial.println();
  // Serial.print("Voltage1 (V): ");
  // Serial.println(usb.getVoltage(1));
  // Serial.print("Current1 (A): ");
  // Serial.println(usb.getCurrent(1));
  // Serial.print("Lower Voltage Tolerance1 (%): ");
  // Serial.println(usb.getLowerVoltageLimit(1));
  // Serial.print("Upper Voltage Tolerance1 (%): ");
  // Serial.println(usb.getUpperVoltageLimit(1));
  // Serial.println();

  // /* Read settings for PDO2 */
  // Serial.print("Voltage2 (V): ");
  // Serial.println(usb.getVoltage(2));
  // Serial.print("Current2 (A): ");
  // Serial.println(usb.getCurrent(2));
  // Serial.print("Lower Voltage Tolerance2 (%): ");
  // Serial.println(usb.getLowerVoltageLimit(2));
  // Serial.print("Upper Voltage Tolerance2 (%): ");
  // Serial.println(usb.getUpperVoltageLimit(2));
  // Serial.println();

  // /* Read settings for PDO3 */
  // Serial.print("Voltage3 (V): ");
  // Serial.println(usb.getVoltage(3));
  // Serial.print("Current3 (A): ");
  // Serial.println(usb.getCurrent(3));
  // Serial.print("Lower Voltage Tolerance3 (%): ");
  // Serial.println(usb.getLowerVoltageLimit(3));
  // Serial.print("Upper Voltage Tolerance3 (%): ");
  // Serial.println(usb.getUpperVoltageLimit(3));
  // Serial.println();

  // /* Read the flex current value */
  // Serial.print("Flex Current: ");
  // Serial.println(usb.getFlexCurrent());

  // /* Read the External Power capable bit */
  // Serial.print("External Power: ");
  // Serial.println(usb.getExternalPower());

  // /* Read the USB Communication capable bit */
  // Serial.print("USB Communication Capable: ");
  // Serial.println(usb.getUsbCommCapable());

  // /* Read the POWER_OK pins configuration */
  // Serial.print("Configuration OK GPIO: ");
  // Serial.println(usb.getConfigOkGpio());

  // /* Read the GPIO pin configuration */
  // Serial.print("GPIO Control: ");
  // Serial.println(usb.getGpioCtrl());

  // /* Read the bit that enables VBUS_EN_SNK pin only when power is greater than 5V */
  // Serial.print("Enable Power Only Above 5V: ");
  // Serial.println(usb.getPowerAbove5vOnly());
  
  // /* Read bit that controls if the Source or Sink device's 
  //    operating current is used in the RDO message */
  // Serial.print("Request Source Current: ");
  // Serial.println(usb.getReqSrcCurrent());

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
