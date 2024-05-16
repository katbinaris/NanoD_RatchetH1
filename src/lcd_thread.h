#pragma once

#include <Arduino.h>
#include "foc_thread.h"
#include "hmi_thread.h"
#include <lvgl.h>
#include "thread_crtp.h"
#include "ui.h"



typedef enum {
    LCD_LAYOUT_DEFAULT = 0x00,
    LCD_LAYOUT_MESSAGE = 0x01,
    LCD_LAYOUT_MENU = 0x02     // TODO future menu mode
} LcdLayoutType;


class LcdCommand {
public:
    LcdLayoutType type = LCD_LAYOUT_DEFAULT;
    String* title = nullptr;
    String* data1 = nullptr;
    String* data2 = nullptr;
    String* data3 = nullptr;
    String* data4 = nullptr;
};



class LcdThread : public Thread<LcdThread> {
    friend class Thread<LcdThread>; //Allow Base Thread to invoke protected run()

    public:
        LcdThread(const uint8_t task_core);
        ~LcdThread();
        
        void put_lcd_command(LcdCommand& cmd);
    
    protected:
        void run();
        void handleLcdCommand();

        LcdCommand last_command;

    private:
        QueueHandle_t _q_lcd_in;
};

extern LcdThread lcd_thread;
