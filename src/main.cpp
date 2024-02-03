#include <Arduino.h>

#include "foc_thread.h"
#include "hmi_thread.h"
#include "lcd_thread.h"
#include "com_thread.h"


static FocThread foc_thread(1); // Handles: Haptic/FOC
static HmiThread hmi_thread(0); // Handles: LED/Keys
static LcdThread lcd_thread(1); // Handles: Display
static ComThread com_thread(0); // Handles: Communication Protocol


void setup() {
  Serial.begin(9600);
  hmi_thread.begin();
  // foc_thread.begin();
  lcd_thread.begin();
  vTaskDelete(NULL);
  
}
void loop() {
  // Main loop left empty
}