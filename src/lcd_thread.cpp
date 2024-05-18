#include <Arduino.h>
#include "lcd_thread.h"



static const uint8_t LEDC_CH_LCD_BKL = 0; // LEDC Channel for LCD Backlight
static uint16_t LEDC_MAX_BLK = 3200; // Maximum Brightness for Active Mode
static uint16_t LEDC_MIN_BLK = LEDC_MAX_BLK / 10; // Minimum Brightness for Idle Mode
static uint16_t DEF_IDLE_TIMEOUT = 20000; // 20 seconds for Idle Timeout


#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8)) // 240*240/10*2 = 11520 bytes for 1/10 screen size
uint32_t draw_buf[DRAW_BUF_SIZE / 4];   // Declare a buffer for drawing


LcdThread::LcdThread(const uint8_t task_core) : Thread("LCD", 8192, 1, task_core) {
    _q_lcd_in = xQueueCreate(2, sizeof( LcdCommand ));
    last_command.type = LCD_LAYOUT_DEFAULT;
    last_command.title = nullptr;
    last_command.data1 = nullptr;
    last_command.data2 = nullptr;
    last_command.data3 = nullptr;
    last_command.data4 = nullptr;
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

/* 
    Tasker for Profile data update - updates every 200ms (5Hz)
    Screen: ui_valueScreen
    Checks for last_command and updates Profile Name and Description
*/
static void lcd_data_handler(lv_timer_t * lcd_cmd_timer) {
    lcd_thread.handleLcdCommand();
    if (lv_scr_act()==ui_valueScreen){

        if (lcd_thread.last_command.type == LCD_LAYOUT_DEFAULT){
            if (lcd_thread.last_command.title==nullptr || lcd_thread.last_command.title->length()==0) {
                lv_obj_add_flag(ui_profileName, LV_OBJ_FLAG_HIDDEN); // Hide Profile Name
            } else {
                lv_obj_remove_flag(ui_profileName, LV_OBJ_FLAG_HIDDEN); // Show Profile Name
                lv_label_set_text_fmt(ui_profileName, "%s", lcd_thread.last_command.title->c_str()); // Set Profile Name
            }
            if (lcd_thread.last_command.data1==nullptr  || lcd_thread.last_command.data1->length()==0) {
                lv_obj_add_flag(ui_profileDesc, LV_OBJ_FLAG_HIDDEN);    // Hide Profile Description
            } else {
                lv_obj_remove_flag(ui_profileDesc, LV_OBJ_FLAG_HIDDEN); // Show Profile Description
                lv_label_set_text_fmt(ui_profileDesc, "%s", lcd_thread.last_command.data1->c_str()); // Set Profile Description
            }
            if (lcd_thread.last_command.data3==nullptr || lcd_thread.last_command.data3->length()==0) {
                lv_obj_add_flag(ui_msgModal2, LV_OBJ_FLAG_HIDDEN); // Hide Modal
            } else {
                static uint32_t startTime = 0;
                static const uint32_t interval = 5000; // 5 seconds

                if (startTime == 0) {
                    startTime = millis();
                    
                    lv_obj_remove_flag(ui_msgModal2, LV_OBJ_FLAG_HIDDEN); // Show Modal
                } else {
                    if (millis() - startTime >= interval) {
                        lv_obj_add_flag(ui_msgModal2, LV_OBJ_FLAG_HIDDEN); // Hide Modal
                        lcd_thread.last_command.data3 = nullptr; // Reset Command
                        startTime = 0; // Reset the start time
                    }
                }
            
            }
        }

        
            
        
        

        
    }
}

/* 
    Tasker for sprite-sheet animation update - updates every 2 seconds - 0.5Hz
    Screen: ui_valueScreen
    Screen: ui_profSelectScreen
    Updates Idle Cat Animation and Profile Selection Arrow Animation
*/
static void idle_anim_handler(lv_timer_t * animtimer) {
    if (lv_scr_act()==ui_valueScreen) // Value Screen
{
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
    fps = (fps + 1) % 4; // 4 Frames every 2 seconds
}
if (lv_scr_act()==ui_profSelectScreen) // Profile Selection Screen
{
    static uint8_t fps = 0;
    switch(fps) {
        case 0:
            lv_obj_set_x( ui_arrInd, 17 );
            break;
        case 1:
            lv_obj_set_x( ui_arrInd, 12 );
            break;
        
    }
    fps = (fps + 1) % 2; // 2 Frames every 2 seconds
}
}

/*
    Tasker for Knob Position tracking - Updates every 16ms (60Hz)
    Screen: ui_valueScreen
    Screen: ui_profSelectScreen
*/

unsigned long lastCheckk = 0;
static void counter_handler(lv_timer_t * postimer) {
    static uint16_t last_pos = -1; // Default Last Position
    static bool idleTime = false; // Default Idle Time
    static bool isIdle = false; // Default Idle Flag
    uint16_t pos = foc_thread.pass_cur_pos(); // Get Current Position from FOC Thread
    uint16_t end_pos = foc_thread.pass_end_pos(); // Get End Position from FOC Thread
    uint16_t last_end_pos;
    

         if (pos == last_pos) {
            unsigned long elapsedTime = millis() - lastCheckk;
            if (elapsedTime >= DEF_IDLE_TIMEOUT) {
                idleTime = true;          
            }
        } else {
        
            idleTime = false;
            if (lv_scr_act()==ui_valueScreen){
                lv_label_set_text_fmt(ui_posind, "%d", pos);
                lv_label_set_text_fmt(ui_posindSha, "%d", pos);
                if (end_pos != last_end_pos) {
                    lv_arc_set_range(ui_Arc1, 0, end_pos);
                    last_end_pos = end_pos;
                    // Don't update arc range if end_pos is same as last_end_pos
                }
                lv_arc_set_value(ui_Arc1, pos);
                last_pos = pos; // Update Last Position
            }
            if (lv_scr_act()==ui_profSelectScreen){
                lv_label_set_text_fmt(ui_pCount, "%d", pos); // Set Position Indicator
                if(last_pos != pos){
                lv_roller_set_selected(ui_profList, pos, LV_ANIM_ON); // Set Roller to Current Position - Animate ON
                last_pos = pos; // Update Last Position
                }
            }

             lastCheckk = millis();
        }
    /*
        TODO: Wrap Idle Anim and Value Screen into containers so that they can be hidden and shown together rather than individually
    */

    if (idleTime == true) {
            if (isIdle == false) {
            lv_obj_remove_flag( ui_IdleCat, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag( ui_IdleCatShadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_dataScreen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_msgModal2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(0x565656), LV_PART_INDICATOR | LV_STATE_DEFAULT );
            ledcWrite(0, LEDC_MIN_BLK); // Set Backlight to Min Brightness
            isIdle = true; // Set Idle Flag
            }
            
    }
    if (idleTime == false) {
            if (isIdle == true) {
            lv_obj_add_flag( ui_IdleCat, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag( ui_IdleCatShadow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag( ui_dataScreen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(0xFF7D00), LV_PART_INDICATOR | LV_STATE_DEFAULT );
            ledcWrite(0, LEDC_MAX_BLK); // Set Backlight to Max Brightness
            isIdle = false; // Reset Idle Flag
            }
    }
}

void LcdThread::run() {
    // Setup LedC
    ledcSetup(0, 5000, 12); // 4096 steps @ 5Khz
    ledcAttachPin(5, 0); // LEDC on Pin 5
    lv_init(); // Initialize LVGL

    /*
        Create Display Object
    */

    lv_display_t * disp;
    disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, sizeof(draw_buf));
    
    /*
        Set timers for LCD Data Handler, Idle Animation Handler and Counter Handler
    */

    
    lv_timer_t * animtimer = lv_timer_create(idle_anim_handler, 2000, NULL);
    lv_timer_t * postimer = lv_timer_create(counter_handler, 16, NULL);
    lv_timer_t * lcd_cmd_timer = lv_timer_create(lcd_data_handler, 50, NULL);

    /* 
        Start Timers
    */

    lv_timer_ready(animtimer);
    lv_timer_ready(postimer);
    lv_timer_ready(lcd_cmd_timer);


    ui_init(); // Initialize UI
    ledcWrite(0, LEDC_MAX_BLK); // Initialize Backlight to Max Brightness
    
    /*s
        Main Loop for LVGL
        Consist only of LVGL Timer Handler and LVGL Tick Increment
    */

    while (1) {        
        lv_timer_handler();
        lv_tick_inc(10);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
};

