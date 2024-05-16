#include <Arduino.h>
#include "lcd_thread.h"


// Define LEDC Channel for LCD backlight control
static const uint8_t LEDC_CH_LCD_BKL = 0;


// LCD Thread only receives data from COM Thread, sends nothing.

#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

String default_title = "NanoD++";
String default_data1 = "Starting up...";
String default_data2 = "Please wait";
String default_data3 = "";
String default_data4 = "";

LcdThread::LcdThread(const uint8_t task_core) : Thread("LCD", 8192, 1, task_core) {
    _q_lcd_in = xQueueCreate(2, sizeof( LcdCommand ));
    last_command.type = LCD_LAYOUT_DEFAULT;
    last_command.title = &default_title;
    last_command.data1 = &default_data1;
    last_command.data2 = &default_data2;
    last_command.data3 = &default_data3;
    last_command.data4 = &default_data4;
};

LcdThread::~LcdThread() {}


void LcdThread::put_lcd_command(LcdCommand& cmd) {
    xQueueSend(_q_lcd_in, &cmd, (TickType_t)0);
};


void LcdThread::handleLcdCommand() {
    LcdCommand cmd;
    if (xQueueReceive(_q_lcd_in, &cmd, (TickType_t)0)) {
        // TODO Implement LCD Command Handling
        last_command = cmd;
    }
};


// TODO: Move to Screen Event
static void idle_anim_handler(lv_timer_t * animtimer) {
    static uint8_t fps = 0;
    switch(fps) {
        case 0:
            lv_label_set_text(ui_IdleCat, "A");
            lv_label_set_text(ui_IdleCatShadow, "E");
            break;
        case 1:
            lv_label_set_text(ui_IdleCat, "B");
            lv_label_set_text(ui_IdleCatShadow, "E");
            break;
        case 2:
            lv_label_set_text(ui_IdleCat, "C");
            lv_label_set_text(ui_IdleCatShadow, "D");
            lv_obj_set_x(ui_IdleCatShadow, -4);
            break;
        case 3:
            lv_label_set_text(ui_IdleCat, "B");
            lv_label_set_text(ui_IdleCatShadow, "E");
            lv_obj_set_x(ui_IdleCatShadow, 0);
            break;
    }
    fps = (fps + 1) % 4;
}

// TODO: move to screen event
static void quote_handler(lv_timer_t * quotetimer) {
    static uint8_t quote = 0;
    switch(quote) {
        case 0:
            lv_label_set_text(ui_IdleQuote, "I MISS YOU!");
            break;
        case 1:
            lv_label_set_text(ui_IdleQuote, "ARE YOU STILL THERE?");
            break;
        case 2:
            lv_label_set_text(ui_IdleQuote, "CAPITAN MEAOW ON STANDBY");
            
            break;

    }
    quote = (quote + 1) % 3;
}

void LcdThread::run() {
    // Setup LedC
    ledcSetup(0, 5000, 12); // 4096 steps @ 5Khz
    ledcAttachPin(5, 0); // LEDC on Pin 5
    // Initalize LVGL
    lv_init();

    lv_display_t * disp;
    disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, sizeof(draw_buf));
    
    
    // lv_timer_t * animtimer = lv_timer_create(idle_anim_handler, 1000, NULL);
    // lv_timer_t * quotetimer = lv_timer_create(quote_handler, 20000, NULL);
    // lv_timer_ready(animtimer);
    // lv_timer_ready(quotetimer);


    ui_init();
    ledcWrite(0, 3000); // Set Max Brightness; TODO: make this software adjustable. MAX VAL 4096
    
    // Main Loop
    while (1) {

        // TODO: Move to screen event as quick rotating and value changes the lvgl slows down
        static uint16_t last_pos = -1;
        uint16_t pos = foc_thread.pass_cur_pos();
        uint16_t end_pos = foc_thread.pass_end_pos();
        
        if (pos != last_pos){
        lv_label_set_text_fmt(ui_posind, "%d", pos);
        lv_label_set_text_fmt(ui_posindSha, "%d", pos);
        lv_arc_set_range(ui_Arc1, 0, end_pos);
        lv_arc_set_value(ui_Arc1, pos);
        last_pos = pos;
        }
        
        lv_task_handler();
        lv_timer_handler();
        lv_tick_inc(5);
        vTaskDelay(1 / portTICK_PERIOD_MS); // LCD Task crashes without it
    }
};
