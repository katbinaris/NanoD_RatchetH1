
#include "HapticProfileManager.h"

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
  // TODO
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
  profile_id = obj["id"].as<int>();
  profile_name = obj["name"].as<String>();
  profile_desc = obj["desc"].as<String>();

  // transfer haptic_config fields
  haptic_config.profile_type = obj["profile_type"].as<int>();
  haptic_config.position_num = obj["position_num"].as<int>();
  haptic_config.attract_distance = obj["attract_distance"].as<float>();
  haptic_config.feedback_strength = obj["feedback_strength"].as<int>();
  haptic_config.bounce_strength = obj["bounce_strength"].as<int>();
  haptic_config.haptic_click_strength = obj["haptic_click_strength"].as<int>();
  haptic_config.output_ramp = obj["output_ramp"].as<int>();

  // TODO transfer LED, key and sound fields


  return *this;
};



void HapticProfile::toJSON(JsonDocument& doc){
  doc["id"] = profile_id;
  doc["name"] = profile_name;
  doc["desc"] = profile_desc;

  // transfer haptic_config fields
  doc["profile_type"] = haptic_config.profile_type;
  doc["position_num"] = haptic_config.position_num;
  doc["attract_distance"] = haptic_config.attract_distance;
  doc["feedback_strength"] = haptic_config.feedback_strength;
  doc["bounce_strength"] = haptic_config.bounce_strength;
  doc["haptic_click_strength"] = haptic_config.haptic_click_strength;
  doc["output_ramp"] = haptic_config.output_ramp;

  // TODO transfer LED, key and sound fields
};
