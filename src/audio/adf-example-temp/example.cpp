/*
#include <adf-i2s.h>

MyI2S myI2S;

const uint8_t sample[] = {  };

void setup() {
    myI2S.begin();
    myI2S.setVolume(80); // Set initial volume to 80%
}

void loop() {
    static unsigned long lastTriggerTime = 0;
    unsigned long currentTime = millis();

    // Trigger sample playback every 100ms (adjust as needed)
    if (currentTime - lastTriggerTime > 100) {
        myI2S.playSample(sample, sizeof(sample));
        lastTriggerTime = currentTime;
    }

    myI2S.handleAudio();
}
*/
