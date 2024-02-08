#include "foc_thread.h"
#include <SimpleFOC.h>
#include "utils.h"
#include "HapticCommander.h"

/*
    FOC Thread runs BLDCHaptic Library in estimated current mode
    It sends and receives data from Com Thread.

*/

FocThread::FocThread(const uint8_t task_core) : Thread("FOC", 2048, 1, task_core) {
    _q_in = xQueueCreate(5, sizeof( String* ));
    _q_out = xQueueCreate(5, sizeof( String* ));
    _q_haptic = xQueueCreate(2, sizeof( hapticParms* ));
    assert(_q_in != NULL);
}

FocThread::~FocThread() {}
   
    BLDCMotor motor = BLDCMotor(7, 5.3);
    BLDCDriver3PWM driver = BLDCDriver3PWM(PIN_IN_U, PIN_IN_V, PIN_IN_W, PIN_EN_U, PIN_EN_V, PIN_EN_W);

    MagneticSensorMT6701SSI encoder(PIN_MAG_CS);
    HapticInterface haptic = HapticInterface(&motor);
    HapticCommander commander = HapticCommander(&motor);

void FocThread::run() {

    SPIClass* spi = new SPIClass(FSPI);
    spi->begin(PIN_MAG_CLK, PIN_MAG_DO, -1, PIN_MAG_CS);
    encoder.init(spi);

    driver.voltage_power_supply = driver_supply;
    driver.voltage_limit = driver_voltage_limit;
    driver.init();
    motor.linkSensor(&encoder);
    motor.linkDriver(&driver);
    motor.LPF_velocity.Tf = 0.01;
    motor.current_limit = 1.22;
    motor.init();
    motor.initFOC();
    haptic.init();

    while (true) {
        haptic.haptic_loop();
        handleMessage();
        handleHapticProfile();
    }
        
};


void FocThread::put_message(String* message) {
    if (message!=nullptr)
        xQueueSend(_q_in, (void*) message, (TickType_t)0);
};

String* FocThread::get_message() {
    String* message = nullptr;
    if (xQueueReceive(_q_out, &message, (TickType_t)0)) {
        return message;
    }
    return nullptr;
};


void FocThread::put_haptic_profile(hapticParms* profile) {
    if (profile!=nullptr)
        xQueueSend(_q_haptic, (void*) profile, (TickType_t)0);
};



void FocThread::handleMessage() {
    String* message = nullptr;
    if (xQueueReceive(_q_in, &message, (TickType_t)0)) {
        if (message!=nullptr) {
            commander.handleMessage(message);
            xQueueSend(_q_out, (void*) message, (TickType_t)0);
        }
    }
};


void FocThread::handleHapticProfile() {
    hapticParms* profile = nullptr;
    if (xQueueReceive(_q_haptic, &profile, (TickType_t)0)) {
        if (profile!=nullptr) {
            // TODO copy over field values
        }
    }
};