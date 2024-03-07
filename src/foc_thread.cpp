#include "./foc_thread.h"
#include "utils.h"
#include "HapticCommander.h"
#include "./com_thread.h"

/*
    FOC Thread runs BLDCHaptic Library in estimated current mode
    It sends and receives data from Com Thread.
*/



// global variables
BLDCMotor motor = BLDCMotor(7, 5.3);
BLDCDriver3PWM driver = BLDCDriver3PWM(PIN_IN_U, PIN_IN_V, PIN_IN_W, PIN_EN_U, PIN_EN_V, PIN_EN_W);

MagneticSensorMT6701SSI encoder(PIN_MAG_CS);
HapticInterface haptic = HapticInterface(&motor);
HapticCommander commander = HapticCommander(&motor);



FocThread::FocThread(const uint8_t task_core) : Thread("FOC", 2048, 1, task_core) {
    _q_motor_in = xQueueCreate(5, sizeof( String* ));
    _q_haptic_in = xQueueCreate(2, sizeof( hapticConfig ));
    _q_angleevt_out = xQueueCreate(5, sizeof( AngleEvt ));
    assert(_q_motor_in != NULL);
    assert(_q_haptic_in != NULL);
    assert(_q_angleevt_out != NULL);
}

FocThread::~FocThread() {}


void FocThread::init(hapticConfig& initialConfig) {
    haptic.haptic_config = initialConfig;
};


void FocThread::run() {

    SPIClass* spi = new SPIClass(HSPI);
    spi->begin(PIN_MAG_CLK, PIN_MAG_DO, -1, PIN_MAG_CS);
    encoder.init(spi);

    driver.voltage_power_supply = 5.0f; // TODO global settings
    driver.voltage_limit = 5.0f;
    driver.init();
    motor.linkSensor(&encoder);
    motor.linkDriver(&driver);
    motor.LPF_velocity.Tf = 0.01;
    motor.current_limit = 1.22;
    motor.init();
    motor.initFOC();
    haptic.init();
    motor.disable(); // TOOD remove me
    
    float lastang = encoder.getAngle();
    unsigned long ts = micros();
    while (true) {
        //haptic.haptic_loop();
        encoder.update(); // TODO remove me when haptic is working
        float ang = encoder.getAngle();
        unsigned long now = micros();
        if (fabs(ang - lastang) >= angleEventMinAngle && now - ts >= angleEventMinMicroseconds) {
            AngleEvt ae = { encoder.getMechanicalAngle(), encoder.getFullRotations(), encoder.getVelocity() };
            xQueueSend(_q_angleevt_out, &ae, (TickType_t)0);
            lastang = ang;
            ts = now;
        }
        
        handleMessage();
        handleHapticConfig();
        vTaskDelay(1);
    }
        
};


void FocThread::put_motor_command(String* message) {
    if (message!=nullptr)
        xQueueSend(_q_motor_in, (void*) message, (TickType_t)0);
};


void FocThread::put_haptic_config(hapticConfig& profile) {
    xQueueSend(_q_haptic_in, &profile, (TickType_t)0);
};


bool FocThread::get_angle_event(AngleEvt* evt) {
    return xQueueReceive(_q_angleevt_out, evt, (TickType_t)0);
};


float FocThread::get_motor_angle() {
    return encoder.getMechanicalAngle();
};



void FocThread::handleMessage() {
    String* message = nullptr;
    if (xQueueReceive(_q_motor_in, &message, (TickType_t)0)) {
        if (message!=nullptr) {
            commander.handleMessage(message);
            StringMessage smsg(message, StringMessageType::STRING_MESSAGE_MOTOR);
            com_thread.put_string_message(smsg); // message String* is returned to comms thread, where it is deleted if necessary
        }
    }
};


void FocThread::handleHapticConfig() {
    if (xQueueReceive(_q_haptic_in, &haptic.haptic_config, (TickType_t)0)) {
        // TODO apply haptic config to motor
    }
};

