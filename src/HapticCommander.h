
#pragma once

#include "comms/SimpleFOCRegisters.h"
#include "BLDCMotor.h"

/**
 * HapticCommander is an implementation of SimpleFOC RegisiterIO that operates on
 * messages received as Strings from the comms thread via ESP32 xQueue.
 * 
 * It provides the bridge between SimpleFOC motor registers and the rest of the
 * comms code.
 */
class HapticCommander : public RegisterIO {
public:
    HapticCommander(BLDCMotor* motor);
    virtual ~HapticCommander() = default;

    void handleMessage(String* message);
    void sendRegister(uint8_t reg);

    RegisterIO& operator<<(float value);
    RegisterIO& operator<<(uint32_t value);
    RegisterIO& operator<<(uint8_t value);
    RegisterIO& operator>>(float& value);
    RegisterIO& operator>>(uint32_t& value);
    RegisterIO& operator>>(uint8_t& value);

protected:
    BLDCMotor* motor;
    char* msg_in;
    String* msg_out;
};
