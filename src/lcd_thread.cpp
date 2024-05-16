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
unsigned long lastCheckk = 0;
static void counter_handler(lv_timer_t * quotetimer) {
    static uint16_t last_pos = -1;
    static bool idleTime = false;
    static bool isIdle = false;
        uint16_t pos = foc_thread.pass_cur_pos();
        uint16_t end_pos = foc_thread.pass_end_pos();
        uint16_t idle_timeout_ms = 20000;
        


         if (pos == last_pos) {
        unsigned long elapsedTime = millis() - lastCheckk;
        if (elapsedTime >= idle_timeout_ms) {
            idleTime = true;          
        }
    } else {
        
        idleTime = false;
        lv_label_set_text_fmt(ui_posind, "%d", pos);
        lv_label_set_text_fmt(ui_posind, "%d", pos);
        lv_label_set_text_fmt(ui_posindSha, "%d", pos);
        lv_arc_set_range(ui_Arc1, 0, end_pos);
        lv_arc_set_value(ui_Arc1, pos);
        
        last_pos = pos;
        lastCheckk = millis();
    }

    if (idleTime == true) {

            if (isIdle == false) {
            lv_obj_remove_flag( ui_IdleCat, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag( ui_IdleCatShadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_posIndicator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_profileName, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_profileDesc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(0x565656), LV_PART_INDICATOR | LV_STATE_DEFAULT );
            ledcWrite(0, 320); // Set Max Brightness; TODO: make this software adjustable. MAX VAL 4096
            isIdle = true;
            }
            
    }
    if (idleTime == false) {
            if (isIdle == true) {
            lv_obj_add_flag( ui_IdleCat, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_IdleCatShadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag( ui_posIndicator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag( ui_profileName, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag( ui_profileDesc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(0xFF7D00), LV_PART_INDICATOR | LV_STATE_DEFAULT );
            ledcWrite(0, 3200); // Set Max Brightness; TODO: make this software adjustable. MAX VAL 4096
            isIdle = false;
            }
    }
}



void LcdThread::run() {
    // Setup LedC
    ledcSetup(0, 5000, 12); // 4096 steps @ 5Khz
    ledcAttachPin(5, 0); // LEDC on Pin 5
    // Initalize LVGL
    lv_init();

    lv_display_t * disp;
    disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, sizeof(draw_buf));
    
    
    lv_timer_t * animtimer = lv_timer_create(idle_anim_handler, 2000, NULL);
    lv_timer_t * postimer = lv_timer_create(counter_handler, 16, NULL);
    lv_timer_ready(animtimer);
    lv_timer_ready(postimer);


    ui_init();
    ledcWrite(0, 3000); // Set Max Brightness; TODO: make this software adjustable. MAX VAL 4096
    


    // Main Loop
    while (1) {        
        // lv_task_handler();
        lv_timer_handler();
        lv_tick_inc(10);
        vTaskDelay(1 / portTICK_PERIOD_MS); // LCD Task crashes without it
    }
};

