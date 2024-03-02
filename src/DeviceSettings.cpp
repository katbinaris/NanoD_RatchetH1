
#include "./DeviceSettings.h"
#include <Arduino.h>
#include "nanofoc_d.h"


// global singleton instance
DeviceSettings DeviceSettings::instance = DeviceSettings();


DeviceSettings& DeviceSettings::getInstance() {
    return instance;
};



DeviceSettings::DeviceSettings() {
    // default settings
    debug = false;
    ledMaxBrightness = DEFAULT_LED_MAX_BRIGHTNESS;
    maxVelocity = 10.0;
    maxVoltage = 50.0;
    serialNumber = String(ESP.getEfuseMac(), HEX);
    deviceName = "Nano_" + serialNumber;
    firmwareVersion = String(NANO_FIRMWARE_VERSION);
};


DeviceSettings::~DeviceSettings() {
    // nothing to do here
};


DeviceSettings& DeviceSettings::operator=(JsonObject& obj){
    if (obj["debug"]!=nullptr)
        debug = obj["debug"].as<bool>();
    if (obj["ledMaxBrightness"]!=nullptr)
        ledMaxBrightness = obj["ledMaxBrightness"].as<uint8_t>();
    if (obj["maxVelocity"]!=nullptr)
        maxVelocity = obj["maxVelocity"].as<float>();
    if (obj["maxVoltage"]!=nullptr)
        maxVoltage = obj["maxVoltage"].as<float>();
    if (obj["deviceName"]!=nullptr)
        deviceName = obj["deviceName"].as<String>();
    return *this;
};


void DeviceSettings::toJSON(JsonDocument& doc){
    doc["debug"] = debug;
    doc["ledMaxBrightness"] = ledMaxBrightness;
    doc["maxVelocity"] = maxVelocity;
    doc["maxVoltage"] = maxVoltage;
    doc["deviceName"] = deviceName;
    doc["serialNumber"] = serialNumber;
    doc["firmwareVersion"] = firmwareVersion;
};


void DeviceSettings::toSPIFFS(){
    // TODO implement me!
};


void DeviceSettings::fromSPIFFS(){
    // TODO implement me!
    // note: use of serial: this function is called from setup() in main.cpp, or from the comms thread.
    Serial.println("Loading settings from SPIFFS...");
    //Serial.println("Settings loaded!");
    Serial.println("Settings not found!");
};

