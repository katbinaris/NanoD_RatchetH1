
#include "./HapticProfileManager.h"
#include "./DeviceSettings.h"
#include "SPIFFS.h"


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
              *profile = obj; // TODO this won't default existing field values if they're not present in JSON - need to handle this for when new fields are added
              profile->dirty = (obj["version"].isNull() || obj["version"].as<int>()!=PROFILE_VERSION);
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
        profiles[i].toJSON(doc);
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
#define update_field(prop, field) if (!obj[#prop].isNull()) { field = obj[#prop].as<decltype(field)>(); dirty = true; }



HapticProfile& HapticProfile::operator=(JsonObject& obj) {
  // if (!obj["id"].isNull())
  //   profile_id = obj["id"].as<int>();
  update_field(name, profile_name);
  update_field(desc, profile_desc);
  update_field(profileTag, profile_tag);
  // transfer haptic_config fields
  update_field(profileType, haptic_config.profile_type);
  update_field(position_num, haptic_config.position_num);
  update_field(attract_distance, haptic_config.attract_distance);
  update_field(feedback_strength, haptic_config.feedback_strength);
  update_field(bounce_strength, haptic_config.bounce_strength);
  update_field(haptic_click_strength, haptic_config.haptic_click_strength);
  update_field(output_ramp, haptic_config.output_ramp);
  // led config fields
  update_field(ledEnable, led_config.led_enable);
  update_field(ledBrightness, led_config.led_brightness);
  update_field(ledMode, led_config.led_mode);
  update_field(pointer, led_config.pointer_col);
  update_field(primary, led_config.primary_col);
  update_field(secondary, led_config.secondary_col);
  update_field(buttonAIdle, led_config.button_A_col_idle);
  update_field(buttonBIdle, led_config.button_B_col_idle);
  update_field(buttonCIdle, led_config.button_C_col_idle);
  update_field(buttonDIdle, led_config.button_D_col_idle);
  update_field(buttonAPress, led_config.button_A_col_press);
  update_field(buttonBPress, led_config.button_B_col_press);
  update_field(buttonCPress, led_config.button_C_col_press);
  update_field(buttonDPress, led_config.button_D_col_press);
  // key config fields
  update_field(internalMacro, key_config.internal_macro);
  update_field(knobMap, key_config.knob_map);
  update_field(switchA, key_config.button_A_map);
  update_field(switchB, key_config.button_B_map);
  update_field(switchC, key_config.button_C_map);
  update_field(switchD, key_config.button_D_map);
  // gui config fields
  update_field(guiEnable, gui_enable);


  // TODO midi and sound fields


  return *this;
};



void HapticProfile::toJSON(JsonDocument& doc){
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
  doc["internalMacro"] = key_config.internal_macro;
  doc["knobMap"] = key_config.knob_map;
  doc["switchA"] = key_config.button_A_map;
  doc["switchB"] = key_config.button_B_map;
  doc["switchC"] = key_config.button_C_map;
  doc["switchD"] = key_config.button_D_map;

  doc["guiEnable"] = gui_enable;

  // TODO midi and sound fields


};
