
#pragma once



#include <ArduinoJSON.h>



class DeviceSettings {
public:
    static DeviceSettings& getInstance();

    DeviceSettings& operator=(JsonObject& obj);
    void toJSON(JsonDocument& doc);

    void toSPIFFS();
    void fromSPIFFS();

    bool debug;
    uint8_t ledMaxBrightness;
    float maxVelocity;
    float maxVoltage;
    String deviceName;

    // read-only settings
    String serialNumber;
    String firmwareVersion;
protected:
    DeviceSettings();
    ~DeviceSettings();

    static DeviceSettings instance;
};

