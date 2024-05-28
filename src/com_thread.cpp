
#include "./com_thread.h"
#include "./foc_thread.h"
#include "./hmi_thread.h"
#include "./lcd_thread.h"
#include <esp_task_wdt.h>
#include "./DeviceSettings.h"
#include "audio/audio.h"




ComThread::ComThread(const uint8_t task_core) : Thread("COM", 12000, 1, task_core) {
    _q_strings_in = xQueueCreate(5, sizeof( StringMessage ));
};

ComThread::~ComThread() {

};


void ComThread::put_string_message(const StringMessage& msg){
    xQueueSend(_q_strings_in, &msg, (TickType_t)0);
};



String title = "";
String data1 = "";
String data2 = "";
String data3 = "";
String data4 = "";

void ComThread::run() {
    // serial is initialized in main.cpp, but subsequently used only here
    Serial.println("COM thread started");
    unsigned long ts = millis();
    ts_last_activity = ts;
    JsonDocument idleDoc;
    LcdCommand remoteLcdCommand;
    remoteLcdCommand.type = LCD_LAYOUT_DEFAULT;
    remoteLcdCommand.title = &title;
    remoteLcdCommand.data1 = &data1;
    remoteLcdCommand.data2 = &data2;
    remoteLcdCommand.data3 = &data3;
    remoteLcdCommand.data4 = &data4;
    dispatchSettings();
    dispatchLcdConfig();
    while (true) {
        JsonDocument doc;
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            DeserializationError error = deserializeJson(doc, input);
            if (error) {
                doc.clear();
                sendError("JSON parse error", error.c_str());
                continue;
            }
            ts_last_activity = millis();
            //Serial.println("JSON received");
            JsonVariant profile = doc["profile"];
            JsonVariant v = doc["updates"];
            if (profile.is<String>() || v!=nullptr) { // haptic command
              handleProfileCommand(profile, v);
            }
            if (doc["current"]!=nullptr) { // set current profile
              setCurrentProfile(doc["current"].as<String>());
            }          
            if (doc["R"]!=nullptr) { // motor command
              // send message to FOC thread
              const char* cmd = doc["R"];
              String* cmdstr = new String(cmd);
              foc_thread.put_motor_command(cmdstr);
            }
            v = doc["message"];
            if (v.is<String>()) { // its a message
              // send message to screen
              StringMessage msg{new String(v.as<String>()), STRING_MESSAGE_DEBUG};
              // TODO lcd_thread.put_string_message(msg);
            }
            v = doc["screen"];
            if (v!=nullptr) {
              if (v["title"].is<String>()) title = v["title"].as<String>(); else title = "";
              if (v["data1"].is<String>()) data1 = v["data1"].as<String>(); else data1 = "";
              if (v["data2"].is<String>()) data2 = v["data2"].as<String>(); else data2 = "";
              if (v["data3"].is<String>()) data3 = v["data3"].as<String>(); else data3 = "";
              if (v["data4"].is<String>()) data4 = v["data4"].as<String>(); else data4 = "";
              lcd_thread.put_lcd_command(remoteLcdCommand);
            }
            v = doc["recalibrate"];
            if (v.is<bool>()) { // recalibrate motor
              // enter calibration mode
              if (v.as<bool>()) {
                Serial.println("Recalibrating motor");
                foc_thread.put_motor_command(new String("129=1"));
              }
            }
            v = doc["profiles"];
            if (v!=nullptr) { // list profiles
              handleProfilesCommand(v);
            }
            v = doc["settings"];
            if (v!=nullptr) { // get or set settings
              handleSettingsCommand(v);
            }
            if (doc["save"]) { // save settings and profiles to SPIFFS
              if (doc["save"].as<bool>()==true) {
                DeviceSettings::getInstance().toSPIFFS();
                HapticProfileManager::getInstance().toSPIFFS();
                DeviceSettings::getInstance().storeCurrentProfile(HapticProfileManager::getInstance().getCurrentProfile()->profile_name);
                JsonDocument reply;
                reply["saved"] = true;
                serializeJson(reply, Serial);
                Serial.println(); // add a newline
              }
            }
            if (doc["load"]) { // load settings and profiles from SPIFFS
              if (doc["load"].as<bool>()==true) {
                // first nuke existing profiles
                for (int i=0; i<MAX_PROFILES; i++) {
                  HapticProfile* p = HapticProfileManager::getInstance()[i];
                  if (p!=nullptr) {
                    String name = p->profile_name;
                    HapticProfileManager::getInstance().remove(name);
                  }
                }
                DeviceSettings::getInstance().fromSPIFFS();
                HapticProfileManager::getInstance().fromSPIFFS();
                HapticProfileManager::getInstance().setCurrentProfile(DeviceSettings::getInstance().loadCurrentProfile());
                dispatchSettings();
                dispatchHapticConfig();
                dispatchAudioConfig();
                dispatchLedConfig();
                dispatchHmiConfig();
                dispatchLcdConfig();
              }
            }
        }

        // send any outgoing messages
        handleMessages();

        // send key events
        handleEvents();

        // send idle message
        unsigned long now = millis();
        if (now-ts>1000 && now-ts_last_activity>global_idle_timeout && global_idle_timeout>0) {
          ts = now;          
          idleDoc["idle"] = now-ts_last_activity;
          serializeJson(idleDoc, Serial);
          Serial.println(); // add a newline
        }
        if (now-ts_last_activity<=global_idle_timeout || global_idle_timeout==0)
          global_sleep_flag = false;
        else
          global_sleep_flag = true;

        vTaskDelay(10); // give other threads a chance to run...
    }

};





void ComThread::handleEvents() {
    JsonDocument eventDoc;
    bool hadEvent = false;
    do {
      KeyEvt keyEvt;
      hadEvent = hmi_thread.get_key_event(&keyEvt);
      if (hadEvent) {
        eventDoc.clear();
        eventDoc["ks"] = keyEvt.keyState;
        if (keyEvt.type==0) // AceButton::kEventPressed
          eventDoc["kd"] = keyEvt.keyNum;
        else if (keyEvt.type==1) // AceButton::kEventReleased
          eventDoc["ku"] = keyEvt.keyNum;
        serializeJson(eventDoc, Serial);
        Serial.println(); // add a newline
        ts_last_activity = millis();
      }
    } while (hadEvent);
    do {
      AngleEvt angleEvt;
      hadEvent = foc_thread.get_angle_event(&angleEvt);
      if (hadEvent) {
        eventDoc.clear();
        eventDoc["p"] = angleEvt.cur_pos;
        serializeJson(eventDoc, Serial);
        Serial.println(); // add a newline
        ts_last_activity = millis();
      }
    } while (hadEvent);
};





void ComThread::handleSettingsCommand(JsonVariant s) {
  if (s.isNull()) return;
  if (s.is<String>()) {
    // send the settings
    JsonDocument doc;
    JsonObject obj = doc["settings"].to<JsonObject>();
    DeviceSettings::getInstance().toJSON(obj);
    serializeJson(doc, Serial);
    Serial.println(); // add a newline
  }
  if (s.is<JsonObject>()) {
    JsonObject obj = s.as<JsonObject>();
    DeviceSettings::getInstance() = obj;
    dispatchSettings();
  }
};



void ComThread::handleMessages() {
  StringMessage incoming;
  JsonDocument doc;
  String pName = "";
  if (xQueueReceive(_q_strings_in, &incoming, (TickType_t)0)) {
    HapticProfileManager& pm = HapticProfileManager::getInstance();
    bool sendDoc = false;
    switch(incoming.type) {
      case STRING_MESSAGE_DEBUG:
        if (incoming.message!=nullptr) {
          doc["debug"] = *incoming.message;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_ERROR:
        if (incoming.message!=nullptr) {
          doc["error"] = *incoming.message;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_MOTOR:
        if (incoming.message!=nullptr) {
          doc["r"] = *incoming.message;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_PROFILE:
        if (incoming.message!=nullptr) {
          String s = *incoming.message;
          setCurrentProfile(s);
          doc["current"] = s;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_NEXT_PROFILE:
        pName = pm.getNextProfileName();
        if (pName!="") {
          setCurrentProfile(pName);
          doc["current"] = pName;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_PREV_PROFILE:
        pName = pm.getPrevProfileName();
        if (pName!="")  {
          setCurrentProfile(pName);
          doc["current"] = pName;
          sendDoc = true;
        }
        break;
      default:
        if (incoming.message!=nullptr) {
          Serial.println(*incoming.message);
        }
        break;
    }
    if (sendDoc) {
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
    }
    if (incoming.message!=nullptr) {
      delete incoming.message;
    }
  }
};




void ComThread::handleProfilesCommand(JsonVariant p) {
  if (p.isNull()) return;
  HapticProfileManager& pm = HapticProfileManager::getInstance();
  if (p.is<String>()) {
    String s = p.as<String>();
    if (s=="#all") {
      // send the list of all profile names
      JsonDocument doc;
      JsonArray arr = doc["profiles"].to<JsonArray>();
      for (int i=0; i<pm.size(); i++) {
        arr.add(pm[i]->profile_name);
      }
      doc["current"] = pm.getCurrentProfile()->profile_name;
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
    }
  }
  if (p.is<JsonArray>()) {
    JsonArray arr = p.as<JsonArray>();
    for (int i=0; i<MAX_PROFILES; i++) {
      HapticProfile* p = pm[i];
      if (p!=nullptr) {
        bool found = false;
        for (int i=0; i<arr.size(); i++) {
          if (arr[i].is<String>()) {
            String s = arr[i].as<String>();
            if (s==p->profile_name) {
              found = true;
              break;
            }
          }
        }
        if (!found) {
          Serial.println("Deleting profile "+p->profile_name);
          pm.remove(p->profile_name);
        }
      }
    }
  }
  // TODO reorder profiles
};




bool ComThread::isProfileNameOk(String& name){
  if (name==nullptr)
    return false;
  if (name.length()<1 || name.length()>20)
    return false;
  // TODO check for invalid characters
  return true;
};




void ComThread::sendError(String& error, String* msg){
      JsonDocument doc;
      doc["error"] = error;
      if (msg!=nullptr)
        doc["msg"] = *msg;
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
};
void ComThread::sendError(String& error, String& msg){
  sendError(error, &msg);
};
void ComThread::sendError(const char* error, String& msg) {
  String e = error;
  sendError(e, &msg);
};
void ComThread::sendError(const char* error, const char* msg) {
  String e = error;
  if (msg==nullptr) {
    sendError(e);
  }
  else {
    String m = msg;
    sendError(e, m);
  }
};



void ComThread::handleProfileCommand(JsonVariant profile, JsonVariant updates) {
  if (profile.isNull()&&updates.isNull()) return;
  HapticProfileManager& pm = HapticProfileManager::getInstance();
  HapticProfile* p;
  if (profile.is<String>()) {
    String pname = profile.as<String>();
    if (!isProfileNameOk(pname)) {
      sendError("Invalid profile name", pname);
      return;
    }
    p = pm[pname];
    if (p==nullptr) 
      p = pm.add(pname);
    if (p==nullptr) {
      sendError("Cannot add another profile");
      return;
    }
  }
  else 
    p = pm.getCurrentProfile();

  if (updates.isNull()) {
    JsonDocument doc, profileDoc;
    // send the selected profile
    JsonObject obj = doc["profile"].to<JsonObject>();
    p->toJSON(obj);
    serializeJson(doc, Serial);
    Serial.println(); // add a newline
  }
  else if (updates.is<JsonObject>()) {
    JsonObject obj = updates.as<JsonObject>();
    if (obj["name"].is<String>() && obj["name"].as<String>()!=p->profile_name) {
      String new_name = obj["name"].as<String>();
      if (pm[new_name]!=nullptr) {
        sendError("Profile name already exists");
        return;
      }
    }
    // update the profile
    *p = obj; // assigning the JSON object to the profile will update the profile's fields
    if (p==pm.getCurrentProfile()) {
      dispatchHapticConfig();
      dispatchAudioConfig();
      dispatchLedConfig();
      dispatchHmiConfig();
      dispatchLcdConfig();
    }
  }
};


void ComThread::setCurrentProfile(String name){
  HapticProfile* profile = HapticProfileManager::getInstance().setCurrentProfile(name);
  if (profile!=nullptr) { // if we changed profile, send the new haptic config to the FOC thread
    dispatchHapticConfig();
    dispatchLedConfig();
    dispatchHmiConfig();
    dispatchAudioConfig();
    dispatchLcdConfig();
  }
};


void ComThread::dispatchLedConfig() {
    ledConfig copy = HapticProfileManager::getInstance().getCurrentProfile()->led_config;
    if (copy.led_brightness>DeviceSettings::getInstance().ledMaxBrightness)
      copy.led_brightness = DeviceSettings::getInstance().ledMaxBrightness;
    hmi_thread.put_led_config(copy);
    //hmi_thread.put_key_config(HapticProfileManager::getInstance().getCurrentProfile()->key_config);
};


void ComThread::dispatchHmiConfig() {
    hmi_thread.put_hmi_config(HapticProfileManager::getInstance().getCurrentProfile()->hmi_config);
};

void ComThread::dispatchHapticConfig() {
  if (HapticProfileManager::getInstance().getCurrentProfile()->hmi_config.knob.num>0)
    foc_thread.put_haptic_config(HapticProfileManager::getInstance().getCurrentProfile()->hmi_config.knob.values[0].haptic);
};

void ComThread::dispatchSettings() {
    DeviceSettings& ds = DeviceSettings::getInstance();
    HmiDeviceSettings hmiSettings{
      .ledMaxBrightness = ds.ledMaxBrightness,
      .deviceOrientation = ds.deviceOrientation,
      .midiUsb = ds.midiUsb,
      .midi2 = ds.midi2,
      .midi_sysex_id = ds.midi_sysex_id
    };
    hmi_thread.put_settings(hmiSettings);
    global_idle_timeout = ds.idleTimeout;
};


void ComThread::dispatchAudioConfig() {
    audioPlayer.put_audio_config(HapticProfileManager::getInstance().getCurrentProfile()->audio_config);
};


String autoDescription = "";

String ComThread::generateDescription(HapticProfile& curr) {
  String desc = "";
  if (curr.hmi_config.knob.num>0) {
    switch (curr.hmi_config.knob.values[0].type) {
      case knobValueType::KV_MIDI:
        desc = "MIDI CC ";
        desc += curr.hmi_config.knob.values[0].midi.cc;
        break;
      case knobValueType::KV_GAMEPAD:
        desc = "Gamepad";
        break;
      case knobValueType::KV_MOUSE:
        desc = "Mouse";
        break;
      case knobValueType::KV_ACTIONS:
        desc = "Actions";
        break;
      case knobValueType::KV_DEVICE_PROFILES:
        desc = "Profiles";
        break;
      default:
        desc = "?";
        break;
    }
  }
  else {
    desc = "No Mapping";
  }
  return desc;
};

void ComThread::dispatchLcdConfig() {
    HapticProfile* curr = HapticProfileManager::getInstance().getCurrentProfile();
    LcdCommand cmd;
    cmd.type = LCD_LAYOUT_DEFAULT;
    cmd.title = &curr->profile_name;
    if (curr->profile_desc.length()>0)
      cmd.data1 = &curr->profile_desc;
    else {
      autoDescription = generateDescription(*curr);
      cmd.data1 = &autoDescription;
    }
    cmd.data2 = nullptr;
    cmd.data3 = nullptr;
    cmd.data4 = nullptr;
    lcd_thread.put_lcd_command(cmd);
};
