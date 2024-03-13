
#include "./HapticProfileManager.h"
#include "./DeviceSettings.h"
#include "SPIFFS.h"

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
      profiles[i].haptic_config = hapticConfig();
      profiles[i].led_config = ledConfig();
      profiles[i].hmi_config = hmiConfig(); // TODO init all fields explicitly
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
  // TODO first iterate over the existing profiles and remove any that are not in the current list
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
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory() && String(file.name()).endsWith(".json")) {
      bool found = false;
      for (int i=0; i<MAX_PROFILES; i++) {
        if (profiles[i].profile_name!="") {
          if (String(file.name()).endsWith(profiles[i].profile_name+".json")) {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        Serial.print("Removing deleted profile: ");
        Serial.println(file.name());
        SPIFFS.remove(file.name());
      }
    }
    file.close();
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
        JsonObject obj = doc.as<JsonObject>();
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
  haptic_parms = hapticParms(); // TODO this struct is not used??
  // if (!obj["id"].isNull())
  //   profile_id = obj["id"].as<int>();
  update_field(obj, name, profile_name);
  update_field(obj, desc, profile_desc);
  update_field(obj, profileTag, profile_tag);
  // transfer haptic_config fields
  update_field(obj, profileType, haptic_config.profile_type);
  update_field(obj, position_num, haptic_config.position_num);
  update_field(obj, attract_distance, haptic_config.attract_distance);
  update_field(obj, feedback_strength, haptic_config.feedback_strength);
  update_field(obj, bounce_strength, haptic_config.bounce_strength);
  update_field(obj, haptic_click_strength, haptic_config.haptic_click_strength);
  update_field(obj, output_ramp, haptic_config.output_ramp);
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
      // TODO
  // gui config fields
  update_field(obj, guiEnable, gui_enable);
  // TODO midi and sound fields


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
      // TODO fields
    }
    else if (type=="gamepad") {
      action.type = keyActionType::KA_GAMEPAD;
      // TODO fields
    }
    else if (type=="profile_change") {
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
  doc["profileType"] = haptic_config.profile_type;
  // transfer haptic_config fields
  doc["profile_type"] = haptic_config.profile_type;
  doc["position_num"] = haptic_config.position_num;
  doc["attract_distance"] = haptic_config.attract_distance;
  doc["feedback_strength"] = haptic_config.feedback_strength;
  doc["bounce_strength"] = haptic_config.bounce_strength;
  doc["haptic_click_strength"] = haptic_config.haptic_click_strength;
  doc["output_ramp"] = haptic_config.output_ramp;
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
  // other configs
  doc["guiEnable"] = gui_enable;
  // TODO midi and sound fields ?
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
      // TODO fields
      break;
    case keyActionType::KA_GAMEPAD:
      obj["type"] = "gamepad";
      // TODO fields
      break;
    case keyActionType::KA_PROFILE_CHANGE:
      obj["type"] = "profile_change";
      // TODO fields
      break;
  }      
};


