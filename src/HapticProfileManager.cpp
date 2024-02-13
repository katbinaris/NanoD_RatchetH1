
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




HapticProfile::HapticProfile() {
  profile_name = "";
};



HapticProfile::~HapticProfile() { };






HapticProfile& HapticProfile::operator=(JsonObject& obj) {
  parms = hapticParms();
  profile_name = obj["name"].as<String>();

// TODO transfer remaining fields

  return *this;
};