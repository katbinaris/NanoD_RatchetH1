#include "com_thread.h"
#include "foc_thread.h"


#define DEFAULT_SERIAL_SPEED 460800


extern FocThread* foc_thread;
HapticProfileManager profileManager;


ComThread::ComThread(const uint8_t task_core) : Thread("COM", 2048, 1, task_core) {

};

ComThread::~ComThread() {

};

void ComThread::run() {

    Serial.begin(DEFAULT_SERIAL_SPEED); // doesn't matter, its USB Serial

    while (true) {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            StaticJsonDocument<256> doc; // TODO use heap since StaticJsonDocument is deprecated
            DeserializationError error = deserializeJson(doc, input);
            if (error) {
                // TODO indicate error
                //Serial.println(error.c_str());
                continue;
            }
            JsonVariant p = doc["p"];
            if (p!=nullptr) { // haptic command
              handleHapticCommand(p);
            }
            const char* cmd = doc["R"];
            if (cmd!=nullptr) { // motor command
              // send message to FOC thread
              String* cmdstr = new String(cmd); // TODO: make sure its a copy
              foc_thread->put_message(cmdstr);
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
        String* message = foc_thread->get_message();
        if (message!=nullptr) {
          Serial.println(*message);
          delete message;
        }

    }

};


void ComThread::handleHapticCommand(JsonVariant p) {
  if (p.isNull()) return;
  if (p.is<String>()) {
    String profile = p.as<String>();
    if (profile=="#all") {
 
    }
    else {

    }
  }
  else if (p.is<JsonObject>()) {
    JsonObject obj = p.as<JsonObject>();
    String pName = obj["name"];
    HapticProfile* profile = profileManager[pName];
    if (profile==nullptr) profile = profileManager.add(pName);
    if (profile!=nullptr) {
      *profile = obj; // assigning the JSON object to the profile will update the profile's haptic_config
      if (profile==current_profile) {
        foc_thread->put_haptic_config(&profile->haptic_config); // TODO: make sure its a copy
      }
    }
  }
};
 