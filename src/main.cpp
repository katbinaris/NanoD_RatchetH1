#include <Arduino.h>

#include "foc_thread.h"
#include "hmi_thread.h"
#include "lcd_thread.h"
#include "com_thread.h"

static FocThread foc_thread(1);
static HmiThread hmi_thread(0);
static LcdThread lcd_thread(1);
static ComThread com_thread(0);

void setup() {
  Serial.begin(9600);
  hmi_thread.begin();
  foc_thread.begin();

  vTaskDelete(NULL);
  
}
void loop() {
  // Main loop left empty
}