#include "mt6701.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <math.h>
#include <stdatomic.h>

static const char *TAG = "mt6701";
static spi_device_handle_t s_spi;

// SSI clock. Was a conservative 1 MHz from first bring-up: SYS INFO's LOOP page then showed the
// read taking 41 of the control loop's 55 us (24 us of it clocking 24 bits). The MT6701's SSI
// allows up to 15.625 MHz. Watch the CRC error count (LOOP page) after changing this.
#define MT6701_SPI_HZ (10 * 1000 * 1000)

// Frames whose CRC didn't match, since boot. Counted only: the angle is still used either way,
// until the check is confirmed on hardware (a wrong CRC formula would otherwise freeze the knob).
static _Atomic uint32_t s_crc_errors = 0;

esp_err_t mt6701_init(void) {
    // Encoder gets its own SPI host (SPI2), separate from the display's (SPI3, Phase 4) --
    // decided in DEVELOPMENT_PLAN.md to avoid any bus contention between the control loop's
    // sensor reads and display DMA bursts.
    spi_bus_config_t buscfg = {
        .mosi_io_num = -1, // sensor only outputs data, never reads command bytes
        .miso_io_num = PIN_MAG_DO,
        .sclk_io_num = PIN_MAG_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    // MT6701 SSI is not register-addressed SPI -- it's a continuous serial output, just a
    // fixed-width clocked frame. SPI mode 0, MSB first.
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = MT6701_SPI_HZ,
        .mode = 0,
        .spics_io_num = PIN_MAG_CS,
        .queue_size = 1,
        .flags = 0,
    };
    err = spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }
    // The sensor is the bus's only device and only the control task reads it (mt6701_init()
    // runs in that task): hold the bus for good, so each read skips the driver's bus lock.
    err = spi_device_acquire_bus(s_spi, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "spi_device_acquire_bus failed: %s (reads still work, a little slower)", esp_err_to_name(err));
    }
    return ESP_OK;
}

// MT6701 SSI CRC: X^6 + X + 1 over the 18 data bits (14 angle + 4 status), MSB first, init 0.
static uint8_t CONTROL_HOT crc6(uint32_t data18) {
    uint8_t crc = 0;
    for (int i = 17; i >= 0; i--) {
        uint8_t bit = ((data18 >> i) & 1) ^ ((crc >> 5) & 1);
        crc = (crc << 1) & 0x3F;
        if (bit) crc ^= 0x03;
    }
    return crc;
}

uint32_t mt6701_crc_errors(void) {
    return atomic_load_explicit(&s_crc_errors, memory_order_relaxed);
}

int32_t CONTROL_HOT mt6701_read_angle_raw(void) {
    uint8_t rx[3] = {0};
    spi_transaction_t t = {
        .length = 24,
        .rxlength = 24,
        .tx_buffer = NULL,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi read failed: %s", esp_err_to_name(err));
        return -1;
    }

    // The 24-bit SSI frame: angle [23:10], status [9:6], CRC [5:0].
    uint32_t raw24 = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
    if (crc6(raw24 >> 6) != (raw24 & 0x3F)) {
        atomic_fetch_add_explicit(&s_crc_errors, 1, memory_order_relaxed);
    }
    uint16_t angle14 = (uint16_t)((raw24 >> 10) & 0x3FFF);
    return angle14;
}

float mt6701_read_angle_rad(void) {
    int32_t raw = mt6701_read_angle_raw();
    if (raw < 0) {
        return NAN;
    }
    return ((float)raw / 16384.0f) * (2.0f * (float)M_PI);
}
