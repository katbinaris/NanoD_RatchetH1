#include "com_thread.h"
#include "foc_thread.h"
#include <esp_task_wdt.h>

#define DEFAULT_SERIAL_SPEED 115200


HapticProfileManager profileManager;


ComThread::ComThread(const uint8_t task_core) : Thread("COM", 2048, 1, task_core) {

};

ComThread::~ComThread() {

};

void ComThread::run() {
    Serial.begin(DEFAULT_SERIAL_SPEED);
    Serial.println("COM thread started"); // TODO remove this
    // TODO load profiles from SPIFFS
    // TODO load the current profile from SPIFFS
    // TODO if there are no profiles, create a default one
    // TODO set the active profile to the other threads
    unsigned long ts = millis();
    while (true) {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            StaticJsonDocument<256> doc; // TODO use heap since StaticJsonDocument is deprecated
            DeserializationError error = deserializeJson(doc, input);
            if (error) {
                Serial.println("JSON error"); // TODO remove this
                // TODO indicate error
                //Serial.println(error.c_str());
                continue;
            }
            Serial.println("JSON received"); // TODO remove this
            JsonVariant p = doc["p"];
            if (p!=nullptr) { // haptic command
              handleHapticCommand(p);
            }
            const char* cmd = doc["R"];
            if (cmd!=nullptr) { // motor command
              // send message to FOC thread
              String* cmdstr = new String(cmd);
              foc_thread.put_message(cmdstr);
            }
            cmd = doc["l"];
            if (cmd!=nullptr) { // LED command
              // TODO send message to LED thread
            }
            cmd = doc["k"];
            if (cmd!=nullptr) { // key mapping command
              // TODO send message to keyboard thread
            }
            cmd = doc["m"];
            if (cmd!=nullptr) { // its a message
              // TODO send message to screen
            }
        }

        // send any outgoing messages
        String* message = foc_thread.get_message();
        if (message!=nullptr) {
          Serial.println(*message);
          // TODO wrap in JSON
          delete message;
        }

        if (millis()-ts>1000) {
          ts = millis();
          Serial.println("COM thread running"); // TODO remove this
        }

        vTaskDelay(10); // give other threads a chance to run...
    }

};


void ComThread::handleHapticCommand(JsonVariant p) {
  if (p.isNull()) return;
  if (p.is<String>()) {
    String profile = p.as<String>();
    if (profile=="#all") {
      // send the list of all profile names
      JsonDocument doc;
      JsonArray arr = doc["profiles"].to<JsonArray>();
      for (int i=0; i<profileManager.size(); i++) {
        arr.add(profileManager[i]->profile_name);
      }
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
    }
    else {
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
  }
  else if (p.is<JsonObject>()) {
    JsonObject obj = p.as<JsonObject>();
    String pName = obj["name"];
    HapticProfile* profile = profileManager[pName];
    if (profile==nullptr) profile = profileManager.add(pName);
    if (profile!=nullptr) {
      *profile = obj; // assigning the JSON object to the profile will update the profile's haptic_config
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
