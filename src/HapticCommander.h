
#pragma once

#include "comms/SimpleFOCRegisters.h"
#include "BLDCMotor.h"


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
