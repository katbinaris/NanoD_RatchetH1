#include "mt6701.h"
#include "board_pins.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "mt6701";
static spi_device_handle_t s_spi;

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
    // fixed-width clocked frame. SPI mode 0, MSB first. Conservative 1MHz clock for first
    // bring-up (datasheet allows faster; raise only once reads are verified correct).
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
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
    return ESP_OK;
}

int32_t mt6701_read_angle_raw(void) {
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

    uint32_t raw24 = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
    // MT6701's native SSI frame is 18 bits (14-bit angle + status/parity); reading 24
    // clocks (3 bytes) instead is the standard pragmatic approach -- the sensor just
    // keeps shifting out data, so over-reading is harmless. The 14-bit angle sits in the
    // most-significant bits of the transaction, hence shift right by (24-14)=10.
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
