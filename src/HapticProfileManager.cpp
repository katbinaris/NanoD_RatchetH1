
#include "HapticProfileManager.h"

HapticProfileManager::HapticProfileManager() {
  for (int i=0; i<MAX_PROFILES; i++) {
    profiles[i].name = "";
  }
};


HapticProfileManager::~HapticProfileManager() { };


HapticProfile* HapticProfileManager::add(String name) {
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].name=="") {
      profiles[i].name = name;
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
    if (profiles[i].name==name) {
      return &profiles[i];
    }
  }
  return nullptr;
};



void HapticProfileManager::remove(String name) {
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].name==name) {
      profiles[i].name = "";
      return;
    }
  }
};



int HapticProfileManager::size() {
  int count = 0;
  for (int i=0; i<MAX_PROFILES; i++) {
    if (profiles[i].name!="") {
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
  name = "";
};



HapticProfile::~HapticProfile() { };






HapticProfile& HapticProfile::operator=(JsonObject& obj) {
  parms = hapticParms();
  name = obj["name"].as<String>();

// TODO transfer remaining fields

  return *this;
};