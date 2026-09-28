#pragma once
// LovyanGFX device definition for the NanoFOC_D GC9A01 240x240 round panel.
// Replaces the esp_lcd + esp_lcd_gc9a01 + esp_lvgl_port stack (see DEVELOPMENT_PLAN.md,
// LovyanGFX migration). Backlight is deliberately NOT a member here -- it stays on the
// existing plain-LEDC code in display_task.cpp, unchanged.
//
// Hardware facts carried over from the LVGL bring-up (confirmed there, re-confirm here):
//   - own SPI bus SPI3_HOST (SPI2_HOST belongs to the MT6701 sensor, mt6701.c)
//   - BGR element order -> rgb_order=false (LovyanGFX: false = BGR, true = RGB)
//   - needs INVON -> invert=true
//   - big-endian RGB565 on the wire: owned by LovyanGFX's panel driver, no manual swap
//   - orientation: final visual wants content top -> physical right edge. The LVGL
//     rotation value doesn't transfer. Re-found on hardware with the checkpoint 1
//     setRotation(0..7) test cycle: index 3 is correct (a plain rotation, not one of the
//     mirrored 4..7 -- LovyanGFX's GC9A01 default MADCTL already matches this FPC mount).
//     Baked in as offset_rotation so logical rotation 0 is the correct orientation.
//     Confirmed on hardware, colors (BGR/invert/byte order) too.
#include <LovyanGFX.hpp>
#include "board_pins.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel;
    lgfx::Bus_SPI _bus;

public:
    LGFX(void) {
        {
            auto cfg = _bus.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = LCD_SPI_FREQUENCY_HZ;
            cfg.freq_read = 16000000; // unused (panel not readable), kept at library-safe value
            cfg.spi_3wire = true;     // no MISO wired
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_LCD_SCLK;
            cfg.pin_mosi = PIN_LCD_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc = PIN_LCD_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs = PIN_LCD_CS;
            cfg.pin_rst = PIN_LCD_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = LCD_WIDTH;
            cfg.memory_height = LCD_HEIGHT;
            cfg.panel_width = LCD_WIDTH;
            cfg.panel_height = LCD_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 3; // confirmed on hardware, see header comment
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false; // BGR
            cfg.dlen_16bit = false;
            cfg.bus_shared = false; // nothing else on SPI3_HOST
            _panel.config(cfg);
        }
        setPanel(&_panel);
    }
};
