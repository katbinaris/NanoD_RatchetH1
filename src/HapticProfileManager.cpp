
#include "./HapticProfileManager.h"
#include "./DeviceSettings.h"
#include "SPIFFS.h"
#include "audio/audio_api.h"

#include "class/hid/hid.h"

#define PROFILES_DIRECTORY "/profiles"



HapticProfileManager HapticProfileManager::instance;


HapticProfileManager& HapticProfileManager::getInstance() {
  return instance;
};


HapticProfileManager::HapticProfileManager() {
  for (int i=0; i<MAX_PROFILES; i++) {
    profiles[i].profile_name = "";
  }
};


HapticProfileManager::~HapticProfileManager() { };


HapticProfile* HapticProfileManager::add(String name) {
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].profile_name=="") {
      profiles[i].profile_name = name;
      profiles[i].dirty = true;
      profiles[i].profile_desc = "";
      profiles[i].profile_tag = "";
      profiles[i].led_config = ledConfig();
      profiles[i].hmi_config = hmiConfig(); // TODO init all fields explicitly
      profiles[i].audio_config.audio_file = hard_wav;
      profiles[i].audio_config.key_audio_file = clack_wav;
      profiles[i].audio_config.audio_feedback_lvl = 100;
      profiles[i].gui_enable = false;
      return &profiles[i];
    }
  }
  return nullptr;
};


HapticProfile* HapticProfileManager::operator[](String name) {
  return get(name);
};


HapticProfile* HapticProfileManager::operator[](int index) {
  if (index>=0 && index<MAX_PROFILES) {
    if (profiles[index].profile_name!="")
      return &profiles[index];
  }
  return nullptr;
};



HapticProfile* HapticProfileManager::get(String name) {
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].profile_name==name) {
      return &profiles[i];
    }
  }
  return nullptr;
};



void HapticProfileManager::remove(String name) {
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].profile_name==name) {
      profiles[i].profile_name = "";
      return;
    }
  }
};



int HapticProfileManager::size() {
  int count = 0;
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].profile_name!="") {
      count++;
    }
  }
  return count;
};





HapticProfile* HapticProfileManager::setCurrentProfile(String name){
  HapticProfile* profile = get(name);
  if (profile!=nullptr) {
    current_profile = profile;
  }
  return current_profile;
};


HapticProfile* HapticProfileManager::getCurrentProfile() {
  return current_profile;
};





void HapticProfileManager::fromSPIFFS() {
  Serial.println("Loading profiles from SPIFFS...");
  // load profiles from SPIFFS
  int count = 0;
  File dir = SPIFFS.open(PROFILES_DIRECTORY, "r");
  if (dir) {
    File file = dir.openNextFile();
    while (file) {
      if (!file.isDirectory() && String(file.name()).endsWith(".json")) {
          Serial.print("Loading profile: ");
          Serial.println(file.name());
          // load the profile
          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, file);
          if (error) {
            Serial.print("ERROR: Failed to parse profile: ");
            Serial.println(file.name());
          }
          else {
            HapticProfile* profile = add(doc["name"].as<String>());
            if (profile!=nullptr) {
              Serial.print("Added profile: ");
              Serial.println(profile->profile_name);
              JsonObject obj = doc.as<JsonObject>();
              *profile = obj;
              profile->dirty = (obj["version"].isNull() || obj["version"].as<int>()!=PROFILE_VERSION);
              if (profile->dirty && obj["version"].is<int>())
                updateProfile(profile, obj["version"].as<int>());
              if (current_profile==nullptr)
                current_profile = profile; // set first loaded profile as current TODO remember last profile used
              count++;
            }
            else {
              Serial.print("ERROR: Failed to add profile: ");
              Serial.println(file.name());
            }
          }
      }
      file.close();
      file = dir.openNextFile();
    }
    dir.close();
  }
  if (count==0) {
    Serial.println("No profiles found.");
    // add a default profile
    HapticProfile* profile = add("Default Profile"); // structs are initialized with default values
    if (profile!=nullptr) {
      Serial.print("Added profile ");
      Serial.println(profile->profile_name);
      // only for the default profile, set a default key-mapping
      profile->hmi_config.keys[0].num_pressed_actions = 1;
      profile->hmi_config.keys[0].pressed[0].type = keyActionType::KA_KEY;
      profile->hmi_config.keys[0].pressed[0].hid.num = 1;
      profile->hmi_config.keys[0].pressed[0].hid.key_codes[0] = HID_KEY_N;
      profile->hmi_config.keys[1].num_pressed_actions = 1;
      profile->hmi_config.keys[1].pressed[0].type = keyActionType::KA_KEY;
      profile->hmi_config.keys[1].pressed[0].hid.num = 1;
      profile->hmi_config.keys[1].pressed[0].hid.key_codes[0] = HID_KEY_A;
      profile->hmi_config.keys[2].num_pressed_actions = 1;
      profile->hmi_config.keys[2].pressed[0].type = keyActionType::KA_KEY;
      profile->hmi_config.keys[2].pressed[0].hid.num = 1;
      profile->hmi_config.keys[2].pressed[0].hid.key_codes[0] = HID_KEY_N;
      profile->hmi_config.keys[3].num_pressed_actions = 1;
      profile->hmi_config.keys[3].pressed[0].type = keyActionType::KA_KEY;
      profile->hmi_config.keys[3].pressed[0].hid.num = 1;
      profile->hmi_config.keys[3].pressed[0].hid.key_codes[0] = HID_KEY_O;
      current_profile = profile;
    }
    else {
      Serial.println("FATAL: Failed to add default profile.");
      while (1);
    }
  }
  else {
    Serial.print(count);
    Serial.println(" profiles loaded.");
  }
};



void HapticProfileManager::toSPIFFS() {
  Serial.println("Saving profiles to SPIFFS...");
  File dir = SPIFFS.open(PROFILES_DIRECTORY, "r");
  if (!dir) {
    Serial.println("Creating profiles directory...");
    if (!SPIFFS.mkdir(PROFILES_DIRECTORY)){
      Serial.println("ERROR: Failed to create profiles directory.");
      return;
    }
    dir = SPIFFS.open(PROFILES_DIRECTORY, "r");
    if (!dir) {
      Serial.println("ERROR: Failed to open profiles directory.");
      return;
    }
  }
  // first iterate over the existing profiles and remove any that are not in the current list
  File file = dir.openNextFile();
  while (file) {
    String filename = file.name();
    if (!file.isDirectory() && String(filename).endsWith(".json")) {
      bool found = false;
      for (int i=0; i<MAX_PROFILES; i++) {
        if (profiles[i].profile_name!="") {
          if (String(filename).endsWith(profiles[i].profile_name+".json")) {
            found = true;
            break;
          }
        }
      }
      file.close();
      if (!found) {
        String remove = PROFILES_DIRECTORY;
        remove += "/" + filename;
        Serial.print("Removing deleted profile: ");
        Serial.println(remove);
        SPIFFS.remove(remove);
      }
    }
    file = dir.openNextFile();
  }
  // then save any dirty profiles to SPIFFS
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].profile_name!="" && profiles[i].dirty) {
      Serial.print("Saving profile: ");
      Serial.println(profiles[i].profile_name);
      String filename = PROFILES_DIRECTORY;
      filename += "/";
      filename += profiles[i].profile_name;
      filename += ".json";
      File file = SPIFFS.open(filename, "w");
      if (file) {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        profiles[i].toJSON(obj);
        serializeJson(doc, file);
        file.close();
        profiles[i].dirty = false;
      }
      else {
        Serial.print("ERROR: Failed to save profile: ");
        Serial.println(profiles[i].profile_name);
      }
    }
  }
};












HapticProfile::HapticProfile() {
  profile_name = "";
};



HapticProfile::~HapticProfile() { };


// bit of pre-processor magic makes the update code more compact
#define update_field(obj, prop, field) if (!obj[#prop].isNull()) { field = obj[#prop].as<decltype(field)>(); dirty = true; }



HapticProfile& HapticProfile::operator=(JsonObject& obj) {
  // if (!obj["id"].isNull())
  //   profile_id = obj["id"].as<int>();
  update_field(obj, name, profile_name);
  update_field(obj, desc, profile_desc);
  update_field(obj, profileTag, profile_tag);
  // led config fields
  update_field(obj, ledEnable, led_config.led_enable);
  update_field(obj, ledBrightness, led_config.led_brightness);
  update_field(obj, ledMode, led_config.led_mode);
  update_field(obj, pointer, led_config.pointer_col);
  update_field(obj, primary, led_config.primary_col);
  update_field(obj, secondary, led_config.secondary_col);
  update_field(obj, buttonAIdle, led_config.button_A_col_idle);
  update_field(obj, buttonBIdle, led_config.button_B_col_idle);
  update_field(obj, buttonCIdle, led_config.button_C_col_idle);
  update_field(obj, buttonDIdle, led_config.button_D_col_idle);
  update_field(obj, buttonAPress, led_config.button_A_col_press);
  update_field(obj, buttonBPress, led_config.button_B_col_press);
  update_field(obj, buttonCPress, led_config.button_C_col_press);
  update_field(obj, buttonDPress, led_config.button_D_col_press);
  // key config fields
  if (!obj["keys"].isNull()) {
    JsonArray keys = obj["keys"].as<JsonArray>();
    for (int i=0; i<4; i++) {
      if (!keys[i].isNull()) {
        JsonObject key = keys[i].as<JsonObject>();
        if (!key["pressed"].isNull()) {
          JsonArray actions = key["pressed"].as<JsonArray>();
          hmi_config.keys[i].num_pressed_actions = min((int)actions.size(), MAX_KEY_ACTIONS);
          for (int j=0;j<hmi_config.keys[i].num_pressed_actions;j++) {
            JsonObject obj = actions[j].as<JsonObject>();
            keyActionFromJSON(obj, hmi_config.keys[i].pressed[j]);
          }
        }
        if (!key["released"].isNull()) {
          JsonArray actions = key["released"].as<JsonArray>();
          hmi_config.keys[i].num_released_actions = min((int)actions.size(), MAX_KEY_ACTIONS);
          for (int j=0;j<hmi_config.keys[i].num_released_actions;j++) {
            JsonObject obj = actions[j].as<JsonObject>();
            keyActionFromJSON(obj, hmi_config.keys[i].released[j]);
          }
        }
        if (!key["held"].isNull()) {
          JsonArray actions = key["held"].as<JsonArray>();
          hmi_config.keys[i].num_held_actions = min((int)actions.size(), MAX_KEY_ACTIONS);
          for (int j=0;j<hmi_config.keys[i].num_held_actions;j++) {
            JsonObject obj = actions[j].as<JsonObject>();
            keyActionFromJSON(obj, hmi_config.keys[i].held[j]);
          }
        }
      }
    }
  }
  if (obj["knob"].is<JsonArray>()) {
    JsonArray values = obj["knob"].as<JsonArray>();
    hmi_config.knob.num = min((int)values.size(), MAX_KNOB_VALUES);
    for (int i=0;i<hmi_config.knob.num;i++) {
      JsonObject value = values[i].as<JsonObject>();
      if (value["type"].is<String>()) {
        update_field(value, valueMin, hmi_config.knob.values[i].value_min);
        update_field(value, valueMax, hmi_config.knob.values[i].value_max);
        update_field(value, angleMin, hmi_config.knob.values[i].angle_min);
        update_field(value, angleMax, hmi_config.knob.values[i].angle_max);
        update_field(value, wrap, hmi_config.knob.values[i].wrap);
        update_field(value, step, hmi_config.knob.values[i].step);
        update_field(value, keyState, hmi_config.knob.values[i].key_state);
        // haptics fields
        JsonObject haptic = value["haptic"].as<JsonObject>();
        if (haptic!=nullptr) {
          update_field(haptic, mode, hmi_config.knob.values[i].haptic.mode);
          update_field(haptic, startPos, hmi_config.knob.values[i].haptic.start_pos);
          update_field(haptic, endPos, hmi_config.knob.values[i].haptic.end_pos);
          update_field(haptic, detentCount, hmi_config.knob.values[i].haptic.detent_count);
          update_field(haptic, vernier, hmi_config.knob.values[i].haptic.vernier);
        }
        String type = value["type"].as<String>();
        if (type=="midi") {
          hmi_config.knob.values[i].type = knobValueType::KV_MIDI;
          update_field(value, channel, hmi_config.knob.values[i].midi.channel);
          update_field(value, cc, hmi_config.knob.values[i].midi.cc);
        }
        else if (type=="mouse") {
          hmi_config.knob.values[i].type = knobValueType::KV_MOUSE;
        }
        else if (type=="gamepad") {
          hmi_config.knob.values[i].type = knobValueType::KV_GAMEPAD;
        }
        else if (type=="actions") {
          hmi_config.knob.values[i].type = knobValueType::KV_ACTIONS;
          if (value["every"].is<JsonObject>()) {
            JsonObject o = value["every"].as<JsonObject>();
            keyActionFromJSON(o, hmi_config.knob.values[i].actions.every);
          }
          else
            hmi_config.knob.values[i].actions.every.type = keyActionType::KA_NONE;
          if (value["cw"].is<JsonObject>()) {
            JsonObject o = value["cw"].as<JsonObject>();
            keyActionFromJSON(o, hmi_config.knob.values[i].actions.cw);
          }
          else
            hmi_config.knob.values[i].actions.cw.type = keyActionType::KA_NONE;
          if (value["ccw"].is<JsonObject>()) {
            JsonObject o = value["ccw"].as<JsonObject>();
            keyActionFromJSON(o, hmi_config.knob.values[i].actions.ccw);
          }
          else
            hmi_config.knob.values[i].actions.ccw.type = keyActionType::KA_NONE;
        }
        else if (type="profiles") {
          hmi_config.knob.values[i].type = knobValueType::KV_DEVICE_PROFILES;
          // TODO fields
        }
      }
    }
  }

  // gui config fields
  update_field(obj, guiEnable, gui_enable);

  // sound-related fields
  if (obj["audio"].is<JsonObject>()) {
    JsonObject audio = obj["audio"].as<JsonObject>();
    if (audio["clickType"].is<String>()) {
      audio_config.audio_file = get_audio_file(audio["clickType"].as<String>());
    }
    if (audio["keyClickType"].is<String>()) {
      audio_config.key_audio_file = get_audio_file(audio["keyClickType"].as<String>());
    }
    if (audio["clickLevel"].is<int>()) {
      audio_config.audio_feedback_lvl = audio["clickLevel"].as<int>();
      if (audio_config.audio_feedback_lvl>125)
        audio_config.audio_feedback_lvl = 125;
      if (audio_config.audio_feedback_lvl<0)
        audio_config.audio_feedback_lvl = 0;
    }
  }

  return *this;
};


void HapticProfile::keyActionFromJSON(JsonObject& obj, keyAction& action) {
  if (!obj["type"].isNull()) {
    String type = obj["type"].as<String>();
    if (type=="midi") {
      action.type = keyActionType::KA_MIDI;
      update_field(obj, channel, action.midi.channel);
      update_field(obj, cc, action.midi.cc);
      update_field(obj, val, action.midi.val);
    }
    else if (type=="key") {
      action.type = keyActionType::KA_KEY;
      if (!obj["keyCodes"].isNull()) {
        JsonArray keys = obj["keyCodes"].as<JsonArray>();
        action.hid.num = min((int)keys.size(), MAX_KEY_KEYCODES);
        for (int k=0; k<action.hid.num; k++) {
          action.hid.key_codes[k] = keys[k].as<uint8_t>();
        }
      }
    }
    else if (type=="mouse") {
      action.type = keyActionType::KA_MOUSE;
      if (obj["buttons"].is<uint8_t>()) {
        action.mouse.buttons = obj["buttons"].as<uint8_t>();
      }
      else {
        action.mouse.buttons = 0;
      }
    }
    else if (type=="gamepad") {
      action.type = keyActionType::KA_GAMEPAD;
      if (obj["buttons"].is<uint8_t>()) {
        action.pad.buttons = obj["buttons"].as<uint8_t>();
      }
      else {
        action.pad.buttons = 0;
      }
    }
    else if (type=="profiles") {
      action.type = keyActionType::KA_PROFILE_CHANGE;
      // TODO fields
    }
  }
};




void HapticProfile::toJSON(JsonObject& doc){
  doc["version"] = PROFILE_VERSION;
  doc["name"] = profile_name;
  doc["desc"] = profile_desc;
  doc["profileTag"] = profile_tag;
  // transfer led_config fields
  doc["ledEnable"] = led_config.led_enable;
  doc["ledBrightness"] = led_config.led_brightness;
  doc["ledMode"] = led_config.led_mode;
  doc["pointer"] = led_config.pointer_col;
  doc["primary"] = led_config.primary_col;
  doc["secondary"] = led_config.secondary_col;
  doc["buttonAIdle"] = led_config.button_A_col_idle;
  doc["buttonBIdle"] = led_config.button_B_col_idle;
  doc["buttonCIdle"] = led_config.button_C_col_idle;
  doc["buttonDIdle"] = led_config.button_D_col_idle;
  doc["buttonAPress"] = led_config.button_A_col_press;
  doc["buttonBPress"] = led_config.button_B_col_press;
  doc["buttonCPress"] = led_config.button_C_col_press;
  doc["buttonDPress"] = led_config.button_D_col_press;
  // transfer key_config fields
  JsonArray keys = doc["keys"].to<JsonArray>();
  for (int i=0; i<4; i++) {
    JsonObject key = keys.add<JsonObject>();
    if (hmi_config.keys[i].num_pressed_actions>0) {
      JsonArray actions = key["pressed"].to<JsonArray>();
      for (int j=0;j<hmi_config.keys[i].num_pressed_actions;j++) {
        if (hmi_config.keys[i].pressed[j].type!=keyActionType::KA_NONE) {
          JsonObject obj = actions.add<JsonObject>();
          keyActionToJSON(obj, hmi_config.keys[i].pressed[j]);
        }
      }
    } // if num_pressed_actions>0
    if (hmi_config.keys[i].num_released_actions>0) {
      JsonArray actions = key["released"].to<JsonArray>();
      for (int j=0;j<hmi_config.keys[i].num_released_actions;j++) {
        if (hmi_config.keys[i].released[j].type!=keyActionType::KA_NONE) {
          JsonObject obj = actions.add<JsonObject>();
          keyActionToJSON(obj, hmi_config.keys[i].released[j]);
        }
      }
    } // if num_released_actions>0
    if (hmi_config.keys[i].num_held_actions>0) {
      JsonArray actions = key["held"].to<JsonArray>();
      for (int j=0;j<hmi_config.keys[i].num_held_actions;j++) {
        if (hmi_config.keys[i].held[j].type!=keyActionType::KA_NONE) {
          JsonObject obj = actions.add<JsonObject>();
          keyActionToJSON(obj, hmi_config.keys[i].held[j]);
        }
      }
    } // if num_held_actions>0
  }// end keys loop
  JsonArray knob = doc["knob"].to<JsonArray>();
  for (int i=0;i<hmi_config.knob.num;i++) {
    JsonObject value = knob.add<JsonObject>();
    value["valueMin"] = hmi_config.knob.values[i].value_min;
    value["valueMax"] = hmi_config.knob.values[i].value_max;
    value["angleMin"] = hmi_config.knob.values[i].angle_min;
    value["angleMax"] = hmi_config.knob.values[i].angle_max;
    value["wrap"] = hmi_config.knob.values[i].wrap;
    value["step"] = hmi_config.knob.values[i].step;
    value["keyState"] = hmi_config.knob.values[i].key_state;
    // haptics fields
    JsonObject haptic = value["haptic"].to<JsonObject>();
    haptic["mode"] = hmi_config.knob.values[i].haptic.mode;
    haptic["startPos"] = hmi_config.knob.values[i].haptic.start_pos;
    haptic["endPos"] = hmi_config.knob.values[i].haptic.end_pos;
    haptic["detentCount"] = hmi_config.knob.values[i].haptic.detent_count;
    haptic["vernier"] = hmi_config.knob.values[i].haptic.vernier;
    switch (hmi_config.knob.values[i].type) {
      case knobValueType::KV_MIDI:
        value["type"] = "midi";
        value["channel"] = hmi_config.knob.values[i].midi.channel;
        value["cc"] = hmi_config.knob.values[i].midi.cc;
        break;
      case knobValueType::KV_MOUSE:
        value["type"] = "mouse";
        value["axis"] = hmi_config.knob.values[i].mouse.axis;
        break;
      case knobValueType::KV_GAMEPAD:
        value["type"] = "gamepad";
        value["axis"] = hmi_config.knob.values[i].pad.axis;
        break;
      case knobValueType::KV_ACTIONS:
        value["type"] = "actions";
        if (hmi_config.knob.values[i].actions.every.type!=keyActionType::KA_NONE) {
          JsonObject every = value["every"].to<JsonObject>();
          keyActionToJSON(every, hmi_config.knob.values[i].actions.every);
        }
        if (hmi_config.knob.values[i].actions.cw.type!=keyActionType::KA_NONE) {
          JsonObject cw = value["cw"].to<JsonObject>();
          keyActionToJSON(cw, hmi_config.knob.values[i].actions.cw);
        }
        if (hmi_config.knob.values[i].actions.ccw.type!=keyActionType::KA_NONE) {
          JsonObject ccw = value["ccw"].to<JsonObject>();
          keyActionToJSON(ccw, hmi_config.knob.values[i].actions.ccw);
        }
        break;
      case knobValueType::KV_DEVICE_PROFILES:
        value["type"] = "profiles";
        // TODO fields
        break;
    }
  }
  // other configs
  doc["guiEnable"] = gui_enable;
  // sound fields
  JsonObject audio = doc["audio"].to<JsonObject>();
  audio["clickType"] = get_audio_filename(audio_config.audio_file);
  audio["keyClickType"] = get_audio_filename(audio_config.key_audio_file);
  audio["clickLevel"] = audio_config.audio_feedback_lvl;
};




void HapticProfile::keyActionToJSON(JsonObject& obj, keyAction& action){
  switch (action.type) {
    case keyActionType::KA_MIDI:
      obj["type"] = "midi";
      obj["channel"] = action.midi.channel;
      obj["cc"] = action.midi.cc;
      obj["val"] = action.midi.val;
      break;
    case keyActionType::KA_KEY:
      {
        obj["type"] = "key";
        JsonArray keys = obj["keyCodes"].to<JsonArray>();
        for (int i=0; i<action.hid.num; i++) {
          keys.add(action.hid.key_codes[i]);
        }
      }
      break;
    case keyActionType::KA_MOUSE:
      obj["type"] = "mouse";
      obj["buttons"] = action.mouse.buttons;
      break;
    case keyActionType::KA_GAMEPAD:
      obj["type"] = "gamepad";
      obj["buttons"] = action.pad.buttons;
      break;
    case keyActionType::KA_PROFILE_CHANGE:
      obj["type"] = "profiles";
      // TODO fields
      break;
  }      
};


