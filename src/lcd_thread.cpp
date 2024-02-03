#pragma once

#include "lcd_thread.h"

// Define LEDC Channel for LCD backlight control
static const uint8_t LEDC_CH_LCD_BKL = 0;
TFT_eSPI lcd_ = TFT_eSPI();
TFT_eSprite spr_ = TFT_eSprite(&lcd_);

// LCD Thread only receives data from COM Thread, sends nothing.

#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t wh = w*h;
    lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    lv_disp_flush_ready(disp);
}

LcdThread::LcdThread(const uint8_t task_core) : Thread("LCD", 4096, 1, task_core) {}

LcdThread::~LcdThread() {}


void LcdThread::run() {
   
    // Setup LedC
    ledcSetup(0, 5000, 12); // ESP32S3 Claims 13 bits for LEDC, 12bit works stable
    ledcAttachPin(5, 0);
    ledcWrite(0, UINT16_MAX); // Set Max Brightness; TODO: make this software adjustable.
    //Force TFT_eSPI to use DMA with S3
    lcd_.initDMA();
    // 3 Sec waiting time in case of stack overflow - dev only
    delay(3000);

    // Initalize LVGL
    lv_init();

    lv_display_t * disp;
    disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, sizeof(draw_buf));
    
    // Set Canvas BG Color
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_STATE_DEFAULT);

    // TODO: Split GUI Partials into separate functions and files(?)
    // Description Label
    lv_obj_t *prof_desc = lv_label_create (lv_scr_act() );
    lv_label_set_text ( prof_desc, "KORG MINILOGUE HIGH PASS FILTER ADJUSTMENT [0...127]");
    lv_obj_set_width( prof_desc, 150);
    lv_label_set_long_mode( prof_desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align( prof_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align ( prof_desc, LV_ALIGN_CENTER, 0, 48);
    lv_obj_set_style_text_color( prof_desc, lv_color_hex(0x4a4a4a), LV_STATE_DEFAULT);

    // Profile Name
    lv_obj_t *prof_name = lv_label_create (lv_scr_act());
    lv_label_set_long_mode( prof_name, LV_LABEL_LONG_SCROLL_CIRCULAR);     /*Circular scroll*/
    lv_label_set_text ( prof_name, "HIGH PASS FILTER FOR FOR HE OSCILLATOR EIGHT");
    lv_obj_set_width( prof_name, 150);
    lv_obj_set_style_text_align( prof_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align ( prof_name, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color( prof_name , lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font( prof_name, &lv_font_unscii_16, 0);

    // Position Indicator
    lv_obj_t *pos_ind = lv_label_create (lv_scr_act());
    lv_label_set_text ( pos_ind, "156");
    lv_obj_set_width( pos_ind, 150);
    lv_obj_set_style_text_align( pos_ind, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align ( pos_ind, LV_ALIGN_CENTER, 0, -48);
    lv_obj_set_style_text_color( pos_ind , lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font( pos_ind, &lv_font_montserrat_48, 0);

    while (1) {
    lv_task_handler();
    lv_tick_inc(5);
    delay(5);
    }
}
