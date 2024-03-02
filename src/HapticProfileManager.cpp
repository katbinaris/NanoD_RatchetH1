
#include "./HapticProfileManager.h"
#include "./DeviceSettings.h"


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



void HapticProfileManager::fromSPIFFS() {
  Serial.println("Loading profiles from SPIFFS...");
  // TODO load profiles from SPIFFS
  Serial.println("No profiles found.");
  // add a default profile 
  // TODO make conditional on no profiles found
  HapticProfile* profile = add("Default Profile"); // structs are initialized with default values
  if (profile!=nullptr) {
    Serial.print("Added profile ");
    Serial.println(profile->profile_name);
    current_profile = &profiles[0];
  }
  else {
    Serial.println("FATAL: Failed to add default profile.");
    while (1);
  }
};


void HapticProfileManager::toSPIFFS() {
  // TODO
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



HapticProfile::HapticProfile() {
  profile_name = "";
};



HapticProfile::~HapticProfile() { };






HapticProfile& HapticProfile::operator=(JsonObject& obj) {
  haptic_parms = hapticParms(); // TODO this struct is not used??
  if (!obj["id"].isNull())
    profile_id = obj["id"].as<int>();
  if (!obj["name"].isNull())
    profile_name = obj["name"].as<String>();
  if (!obj["desc"].isNull())
    profile_desc = obj["desc"].as<String>();
  if (!obj["profileTag"].isNull())
    profile_tag = obj["profileTag"].as<String>();

  // transfer haptic_config fields
  if (!obj["profileType"].isNull())
    haptic_config.profile_type = obj["profileType"].as<int>();
  if (!obj["position_num"].isNull())
    haptic_config.position_num = obj["position_num"].as<int>();
  if (!obj["attract_distance"].isNull())
    haptic_config.attract_distance = obj["attract_distance"].as<float>();
  if (!obj["feedback_strength"].isNull())
    haptic_config.feedback_strength = obj["feedback_strength"].as<int>();
  if (!obj["bounce_strength"].isNull()) 
    haptic_config.bounce_strength = obj["bounce_strength"].as<int>();
  if (!obj["haptic_click_strength"].isNull())
    haptic_config.haptic_click_strength = obj["haptic_click_strength"].as<int>();
  if (!obj["output_ramp"].isNull())    
    haptic_config.output_ramp = obj["output_ramp"].as<int>();

  // led config fields
  if (!obj["ledEnable"].isNull())
    led_config.led_enable = obj["ledEnable"].as<bool>();
  if (!obj["ledBrightness"].isNull())
    led_config.led_brightness = obj["ledBrightness"].as<int>();
  if (!obj["ledMode"].isNull())
    led_config.led_mode = obj["ledMode"].as<int>();
  if (!obj["pointer"].isNull())
    led_config.pointer_col = obj["pointer"].as<int>();
  if (!obj["primary"].isNull())
    led_config.primary_col = obj["primary"].as<int>();
  if (!obj["secondary"].isNull())
    led_config.secondary_col = obj["secondary"].as<int>();
  if (!obj["buttonAIdle"].isNull())
    led_config.button_A_col_idle = obj["buttonAIdle"].as<int>();
  if (!obj["buttonBIdle"].isNull())
    led_config.button_B_col_idle = obj["buttonBIdle"].as<int>();
  if (!obj["buttonCIdle"].isNull())
    led_config.button_C_col_idle = obj["buttonCIdle"].as<int>();
  if (!obj["buttonDIdle"].isNull())
    led_config.button_D_col_idle = obj["buttonDIdle"].as<int>();
  if (!obj["buttonAPress"].isNull())
    led_config.button_A_col_press = obj["buttonAPress"].as<int>();
  if (!obj["buttonBPress"].isNull())
    led_config.button_B_col_press = obj["buttonBPress"].as<int>();
  if (!obj["buttonCPress"].isNull())
    led_config.button_C_col_press = obj["buttonCPress"].as<int>();
  if (!obj["buttonDPress"].isNull())
    led_config.button_D_col_press = obj["buttonDPress"].as<int>();

  // key config fields
  if (!obj["internalMacro"].isNull())
    key_config.internal_macro = obj["internalMacro"].as<bool>();
  if (!obj["knobMap"].isNull())
    key_config.knob_map = obj["knobMap"].as<String>();
  if (!obj["switchA"].isNull())
    key_config.button_A_map = obj["switchA"].as<String>();
  if (!obj["switchB"].isNull())
    key_config.button_B_map = obj["switchB"].as<String>();
  if (!obj["switchC"].isNull())
    key_config.button_C_map = obj["switchC"].as<String>();
  if (!obj["switchD"].isNull())
    key_config.button_D_map = obj["switchD"].as<String>();

  if (!obj["guiEnable"].isNull())
    gui_enable = obj["guiEnable"].as<bool>();


  // TODO midi and sound fields


  return *this;
};



void HapticProfile::toJSON(JsonDocument& doc){
  doc["id"] = profile_id;
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
