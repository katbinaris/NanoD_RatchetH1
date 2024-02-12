#include <Arduino.h>

#include "foc_thread.h"
#include "hmi_thread.h"
#include "lcd_thread.h"
#include "com_thread.h"


// static FocThread foc_thread(1); // Handles: Haptic/FOC
// static HmiThread hmi_thread(0); // Handles: LED/Keys
// static LcdThread lcd_thread(0); // Handles: Display
// static ComThread com_thread(0); // Handles: Communication Protocol

HmiThread hmi_thread = HmiThread(0);
FocThread foc_thread = FocThread(1, hmi_thread);
LcdThread lcd_thread = LcdThread(0);

void setup() {
  hmi_thread.begin();
  foc_thread.begin();
  lcd_thread.begin();
  vTaskDelete(NULL);
  
}
void loop() {
  // Main loop left empty
}