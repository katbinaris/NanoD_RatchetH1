#include "foc_thread.h"
#include "utils.h"

/*
    FOC Thread runs BLDCHaptic Library in estimated current mode
    It sends and receives data from Com Thread.

*/

FocThread::FocThread(const uint8_t task_core, HmiThread& hmi_thread) : Thread{"FOC", 4096, 1, task_core}, hmi_thread_(hmi_thread) {
nano_queue_ = xQueueCreate(10, sizeof( hapticState ));
    assert(nano_queue_ != NULL);
}

FocThread::~FocThread() {}
   
    BLDCMotor motor = BLDCMotor(7, 5.3);
    BLDCDriver3PWM driver = BLDCDriver3PWM(PIN_IN_U, PIN_IN_V, PIN_IN_W, PIN_EN_U, PIN_EN_V, PIN_EN_W);

    MagneticSensorMT6701SSI encoder(PIN_MAG_CS);
    HapticInterface haptic = HapticInterface(&motor);
    

void FocThread::run() {

    SPIClass* spi = new SPIClass(HSPI);
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
        xQueueSend(nano_queue_, haptic.haptic_config, portMAX_DELAY);
    }
    
    
}
