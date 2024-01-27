#include "foc_thread.h"
#include <SimpleFOC.h>
#include "utils.h"

/*
    FOC Thread runs BLDCHaptic Library in estimated current mode
    It sends and receives data from Com Thread.

*/

FocThread::FocThread(const uint8_t task_core) : Thread("FOC", 2048, 1, task_core) {
    queue_ = xQueueCreate(5, sizeof( int ));
    assert(queue_ != NULL);
}

FocThread::~FocThread() {}
   
    BLDCMotor motor = BLDCMotor(7, 5.3);
    BLDCDriver3PWM driver = BLDCDriver3PWM(PIN_IN_U, PIN_IN_V, PIN_IN_W, PIN_EN_U, PIN_EN_V, PIN_EN_W);

    MagneticSensorMT6701SSI encoder(PIN_MAG_CS);
    HapticInterface haptic = HapticInterface(&motor);
    

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

    while (1)
    {
        haptic.haptic_loop();
        
    }
    
    
}
