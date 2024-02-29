
#pragma once



#include <ArduinoJSON.h>


/**
 * Device settings.
 * For use from setup() or comms thread only.
 */
class DeviceSettings {
public:
    static DeviceSettings& getInstance();

    DeviceSettings& operator=(JsonObject& obj);
    void toJSON(JsonDocument& doc);

    void toSPIFFS();

    // load settings - call only from main or in comms thread
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

