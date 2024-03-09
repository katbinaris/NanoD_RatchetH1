
#include "./DeviceSettings.h"
#include <Arduino.h>
#include "nanofoc_d.h"
#include "SPIFFS.h"

#define DEVICE_SETTINGS_FILE "/device_settings.json"


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
    dirty = true;
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
    dirty = true;
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


bool DeviceSettings::toSPIFFS(){
    // note: use of serial: this function is called from the comms thread.
    if (dirty) {
        Serial.println("Saving settings to SPIFFS...");
        File file = SPIFFS.open(DEVICE_SETTINGS_FILE, "w");
        if (!file) {
            Serial.println("ERROR: unable to open settings file!");
            return false;
        }
        // create the JSON
        JsonDocument doc;
        toJSON(doc);
        // write the JSON to the file
        serializeJson(doc, file);
        file.close();
        Serial.println("Settings saved");
        dirty = false;
    }
    return true;
};


bool DeviceSettings::fromSPIFFS(){
    // note: use of serial: this function is called from setup() in main.cpp, or from the comms thread.
    Serial.println("Loading settings from SPIFFS...");
    if (SPIFFS.exists(DEVICE_SETTINGS_FILE)) {
        File file = SPIFFS.open(DEVICE_SETTINGS_FILE, "r");
        if (!file) {
            Serial.println("ERROR: unable to open settings file!");
            return false;
        }
        // parse the JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        if (error) {
            Serial.println("ERROR: unable to parse settings file!");
            return false;
        }
        // update the settings
        JsonObject obj = doc.as<JsonObject>();
        *this = obj;
        Serial.println("Settings loaded");
        dirty = false;
    }
    else {
        Serial.println("Settings not found, default settings used...");
    }
    return true;
};

