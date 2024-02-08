

#pragma once


#include "haptic.h"
#include <ArduinoJSON.h>


#define MAX_PROFILES 10




class HapticProfile {
public:
    HapticProfile();
    ~HapticProfile();

    HapticProfile& operator=(JsonObject& obj);

    hapticParms parms;
    String name;
};



class HapticProfileManager {
public:
    HapticProfileManager();
    ~HapticProfileManager();

    HapticProfile* add(String name);
    HapticProfile* operator[](String name);
    HapticProfile* get(String name);
    void remove(String name);
    int size();

    void fromSPIFFS();
    void toSPIFFS();
protected:
    HapticProfile profiles[MAX_PROFILES];
};