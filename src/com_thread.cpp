
#include "./com_thread.h"
#include "./foc_thread.h"
#include <esp_task_wdt.h>
#include "./DeviceSettings.h"




HapticProfileManager profileManager;


ComThread::ComThread(const uint8_t task_core) : Thread("COM", 2048, 1, task_core) {

};

ComThread::~ComThread() {

};

void ComThread::run() {
    // serial is initialized in main.cpp, but subsequently used only here
    Serial.println("COM thread started");

    // TODO load profiles from SPIFFS
    // TODO load the current profile from SPIFFS
    // TODO if there are no profiles, create a default one
    // TODO set the active profile to the other threads
    unsigned long ts = millis();
    unsigned long ts_last_activity = ts;
    JsonDocument idleDoc;
    while (true) {
        JsonDocument doc;
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            DeserializationError error = deserializeJson(doc, input);
            if (error) {
                doc.clear();
                doc["error"] = "JSON parse error";
                doc["msg"] = error.c_str();
                serializeJson(doc, Serial);
                Serial.println(); // add a newline
                continue;
            }
            Serial.println("JSON received"); // TODO remove this
            JsonVariant v = doc["p"];
            if (v!=nullptr) { // haptic command
              handleProfileCommand(v);
            }
            if (doc["current"]!=nullptr) { // set current profile
              setCurrentProfile(doc["current"].as<String>());
            }          
            if (doc["R"]!=nullptr) { // motor command
              // send message to FOC thread
              const char* cmd = doc["R"];
              String* cmdstr = new String(cmd);
              foc_thread.put_message(cmdstr);
            }
            v = doc["message"]; // TOOD can we re-use the JsonVariant in this way?
            if (v!=nullptr) { // its a message
              // TODO send message to screen
            }
            v = doc["recalibrate"];
            if (v!=nullptr) { // recalibrate motor
              // TODO enter calibration mode
            }
            v = doc["profiles"];
            if (v!=nullptr) { // list profiles
              handleProfilesCommand(v);
            }
            v = doc["settings"];
            if (v!=nullptr) { // get or set settings
              handleSettingsCommand(v);
            }
        }

        // send any outgoing messages
        String* message = foc_thread.get_message();
        if (message!=nullptr) {
          Serial.println(*message);
          // TODO wrap in JSON
          delete message;
          ts_last_activity = millis(); // TODO only update for position events
        }

        // send idle message
        unsigned long now = millis();
        if (now-ts>1000 && now-ts_last_activity>1000) {
          ts = now;          
          idleDoc["idle"] = now-ts_last_activity;
          serializeJson(idleDoc, Serial);
          Serial.println(); // add a newline
        }

        vTaskDelay(10); // give other threads a chance to run...
    }

};


void ComThread::handleSettingsCommand(JsonVariant s) {
  if (s.isNull()) return;
  if (s.is<String>()) {
    // send the settings
    JsonDocument doc;
    DeviceSettings::getInstance().toJSON(doc);
    serializeJson(doc, Serial);
    Serial.println(); // add a newline
  }
  if (s.is<JsonObject>()) {
    JsonObject obj = s.as<JsonObject>();
    DeviceSettings::getInstance() = obj;
    // TODO store to SPIFFS
  }
};



void ComThread::handleProfilesCommand(JsonVariant p) {
  if (p.isNull()) return;
  if (p.is<String>()) {
    String s = p.as<String>();
    if (s=="#all") {
      // send the list of all profile names
      JsonDocument doc;
      JsonArray arr = doc["profiles"].to<JsonArray>();
      for (int i=0; i<profileManager.size(); i++) {
        arr.add(profileManager[i]->profile_name);
      }
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
    }
  }
  // TODO reorder and/or delete profiles
};




void ComThread::handleProfileCommand(JsonVariant p) {
  if (p.isNull()) return;
  if (p.is<String>()) {
    String profile = p.as<String>();
    HapticProfile* p = profileManager[profile];
    JsonDocument doc;
    if (p!=nullptr) {
      // send the selected profile
      p->toJSON(doc);
    }
    else {
      // send an error message
      doc["error"] = "Profile not found";
    }
    serializeJson(doc, Serial);
    Serial.println(); // add a newline
  }
  else if (p.is<JsonObject>()) {
    JsonObject obj = p.as<JsonObject>();
    String pName = obj["name"];
    HapticProfile* profile = profileManager[pName];
    if (profile==nullptr) profile = profileManager.add(pName);
    if (profile!=nullptr) {
      *profile = obj; // assigning the JSON object to the profile will update the profile's fields
      if (profile==profileManager.getCurrentProfile()) {
        hapticConfig* copy = new hapticConfig();
        *copy = profile->haptic_config;
        foc_thread.put_haptic_config(copy); // the copy is deleted in the FOC thread
      }
      // TODO store to SPIFFS
    }
  }
};


void ComThread::setCurrentProfile(String name){
  HapticProfile* profile = profileManager.setCurrentProfile(name);
  if (profile!=nullptr) { // if we changed profile, send the new haptic config to the FOC thread
    hapticConfig* copy = new hapticConfig();
    *copy = profile->haptic_config;
    foc_thread.put_haptic_config(copy); // the copy is deleted in the FOC thread
  }
};
