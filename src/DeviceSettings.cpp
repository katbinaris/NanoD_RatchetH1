
#include "./DeviceSettings.h"
#include <Arduino.h>
#include "nanofoc_d.h"
#include "SPIFFS.h"
#include <Preferences.h>
#include <common/foc_utils.h>

#define DEVICE_SETTINGS_FILE "/device_settings.json"


// global singleton instance
DeviceSettings DeviceSettings::instance = DeviceSettings();

Preferences nano_preferences; 


DeviceSettings& DeviceSettings::getInstance() {
    return instance;
};



DeviceSettings::DeviceSettings() {
    // default settings
    debug = false;
    ledMaxBrightness = DEFAULT_LED_MAX_BRIGHTNESS;
    maxVelocity = 10.0;
    maxVoltage = 5.0;
    deviceOrientation = 1;
    serialNumber = String(ESP.getEfuseMac(), HEX);
    deviceName = "Nano_" + serialNumber;
    firmwareVersion = String(NANO_FIRMWARE_VERSION);
    midiUsb = midiSettings();
    midi2 = midiSettings();
    dirty = true;
    wifiSsid = "";
    wifiPassword = "";
    wifiEnabled = false;
    midi_sysex_id = 0x00;
    idleTimeout = 10000;
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
    if (obj["deviceOrientation"].is<uint16_t>())
        deviceOrientation = obj["deviceOrientation"].as<uint16_t>();
    if (obj["deviceName"]!=nullptr)
        deviceName = obj["deviceName"].as<String>();
    if (obj["wifiSsid"]!=nullptr)
        wifiSsid = obj["wifiSsid"].as<String>();
    if (obj["wifiPassword"]!=nullptr)
        wifiPassword = obj["wifiPassword"].as<String>();
    if (obj["wifiEnabled"]!=nullptr)
        wifiEnabled = obj["wifiEnabled"].as<bool>();
    if (obj["midiUsb"]!=nullptr) {
        JsonObject midiUsbObj = obj["midiUsb"].as<JsonObject>();
        if (midiUsbObj["in"]!=nullptr)
            midiUsb.in = midiUsbObj["in"].as<bool>();
        if (midiUsbObj["out"]!=nullptr)
            midiUsb.out = midiUsbObj["out"].as<bool>();
        if (midiUsbObj["thru"]!=nullptr)
            midiUsb.thru = midiUsbObj["thru"].as<bool>();
        if (midiUsbObj["route"]!=nullptr)
            midiUsb.route = midiUsbObj["route"].as<bool>();
        if (midiUsbObj["nano"]!=nullptr)
            midiUsb.nano = midiUsbObj["nano"].as<bool>();
    }
    if (obj["midi2"]!=nullptr) {
        JsonObject midi2Obj = obj["midi2"].as<JsonObject>();
        if (midi2Obj["in"]!=nullptr)
            midi2.in = midi2Obj["in"].as<bool>();
        if (midi2Obj["out"]!=nullptr)
            midi2.out = midi2Obj["out"].as<bool>();
        if (midi2Obj["thru"]!=nullptr)
            midi2.thru = midi2Obj["thru"].as<bool>();
        if (midi2Obj["route"]!=nullptr)
            midi2.route = midi2Obj["route"].as<bool>();
        if (midi2Obj["nano"]!=nullptr)
            midi2.nano = midi2Obj["nano"].as<bool>();
    }
    if (obj["sysexId"].is<uint8_t>())
        midi_sysex_id = obj["sysexId"].as<uint8_t>();
    if (obj["idleTimeout"].is<uint32_t>())
        idleTimeout = obj["idleTimeout"].as<uint32_t>();
    dirty = true;
    return *this;
};



void DeviceSettings::toJSON(JsonObject& obj){
    obj["debug"] = debug;
    obj["ledMaxBrightness"] = ledMaxBrightness;
    obj["maxVelocity"] = maxVelocity;
    obj["maxVoltage"] = maxVoltage;
    obj["deviceOrientation"] = deviceOrientation;
    obj["deviceName"] = deviceName;

    if (wifiSsid.length() > 0)
        obj["wifiSsid"] = wifiSsid;
    if (wifiPassword.length() > 0)
        obj["wifiPassword"] = wifiPassword;
    obj["wifiEnabled"] = wifiEnabled;

    obj["serialNumber"] = serialNumber;
    obj["firmwareVersion"] = firmwareVersion;
    JsonObject midiUsbObj = obj["midiUsb"].to<JsonObject>();
    midiUsbObj["in"] = midiUsb.in;
    midiUsbObj["out"] = midiUsb.out;
    midiUsbObj["thru"] = midiUsb.thru;
    midiUsbObj["route"] = midiUsb.route;
    midiUsbObj["nano"] = midiUsb.nano;
    JsonObject midi2Obj = obj["midi2"].to<JsonObject>();
    midi2Obj["in"] = midi2.in;
    midi2Obj["out"] = midi2.out;
    midi2Obj["thru"] = midi2.thru;
    midi2Obj["route"] = midi2.route;
    midi2Obj["nano"] = midi2.nano;
    obj["sysexId"] = midi_sysex_id;
    obj["idleTimeout"] = idleTimeout;
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
        JsonObject obj = doc.to<JsonObject>();
        toJSON(obj);
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


bool DeviceSettings::init() {
    if (!nano_preferences.begin("nano_D", false)) {
        Serial.println("ERROR: unable to open Preferences!");
        return false;
    }
    if (SPIFFS.begin(true)) {
        Serial.println("SPIFFS mounted successfully");
    }
    else {
        Serial.println("ERROR: SPIFFS mount failed");
        // this is kind of fatal...
        return false;
    }
    return true;
};


void DeviceSettings::storeCalibration(MotorCalibration& cal) {
    nano_preferences.putUChar("direction", cal.direction);
    nano_preferences.putFloat("zero_angle", cal.zero_angle);
};


MotorCalibration DeviceSettings::loadCalibration() {
    MotorCalibration result;
    result.direction = nano_preferences.getUChar("direction", 0);
    result.zero_angle = nano_preferences.getFloat("zero_angle", NOT_SET);
    return result;
};


String DeviceSettings::loadCurrentProfile() {
    String profile = nano_preferences.getString("current_profile", "Default Profile");
    return profile;
};


void DeviceSettings::storeCurrentProfile(String profile) {
    nano_preferences.putString("current_profile", profile);
};
