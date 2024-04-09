#include <Arduino.h>
#include "lcd_thread.h"
#include "foc_thread.h"

// Define LEDC Channel for LCD backlight control
static const uint8_t LEDC_CH_LCD_BKL = 0;


// LCD Thread only receives data from COM Thread, sends nothing.

#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

LcdThread::LcdThread(const uint8_t task_core) : Thread("LCD", 8192, 1, task_core) {}

LcdThread::~LcdThread() {}


void LcdThread::run() {
   
    // Setup LedC
    ledcSetup(0, 5000, 12); // ESP32S3 Claims 13 bits for LEDC, 12bit works stable
    ledcAttachPin(5, 0);
    ledcWrite(0, 0); // Set Max Brightness; TODO: make this software adjustable.
    
    // 3 Sec waiting time in case of stack overflow - dev only
    

    // Initalize LVGL
    lv_init();

    lv_display_t * disp;
    disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, sizeof(draw_buf));
    
    ui_init();
    delay(500);
    ledcWrite(0, UINT16_MAX); // Set Max Brightness; TODO: make this software adjustable.
    // Main Loop
    while (1) {

        uint16_t pos = foc_thread.pass_cur_pos();
        uint16_t end_pos = foc_thread.pass_end_pos();
        lv_task_handler();
        lv_label_set_text_fmt(ui_posind, "%d", pos);
        lv_label_set_text_fmt(ui_posindSha, "%d", pos);
        lv_slider_set_range(ui_slider, 0, end_pos);
        lv_slider_set_value(ui_slider, pos, LV_ANIM_ON);
        lv_tick_inc(5);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
