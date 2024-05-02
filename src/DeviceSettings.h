
#pragma once



#include <ArduinoJSON.h>


typedef struct {
    bool in = true;
    bool out = true;
    bool thru = false;
    bool route = false;
    bool nano = true;
} midiSettings;


typedef struct {
    int8_t direction;
    float zero_angle;
} MotorCalibration;



typedef struct {
    uint8_t ledMaxBrightness;
    uint16_t deviceOrientation;
    midiSettings midiUsb;
    midiSettings midi2;
} HmiDeviceSettings;



/**
 * Device settings.
 * For use from setup() or comms thread only.
 */
class DeviceSettings {
    friend class HmiThread;
    friend class ComThread;
public:
    static DeviceSettings& getInstance();

    DeviceSettings& operator=(JsonObject& obj);
    void toJSON(JsonObject& obj);

    bool toSPIFFS();

    // load settings - call only from main or in comms thread
    bool fromSPIFFS();

    void storeCurrentProfile(String profile);
    String loadCurrentProfile();
    MotorCalibration loadCalibration();
    void storeCalibration(MotorCalibration& cal);
    bool init();

    bool dirty;

    bool debug;
    uint8_t ledMaxBrightness;
    float maxVelocity;
    float maxVoltage;
    String deviceName;
    uint16_t deviceOrientation;
    midiSettings midiUsb;
    midiSettings midi2;

    // read-only settings
    String serialNumber;
    String firmwareVersion;
protected:
    DeviceSettings();
    ~DeviceSettings();

    static DeviceSettings instance;
};

