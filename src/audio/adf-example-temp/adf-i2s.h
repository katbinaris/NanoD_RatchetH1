/*
#ifndef MYI2S_H
#define MYI2S_H

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_common.h"
#include "audio_mixer.h"

class MyI2S {
public:
    MyI2S();
    void begin();
    void playSample(const uint8_t *sample, size_t size);
    void setVolume(int volume);
    void handleAudio();

private:
    void configureI2S();
    static const i2s_port_t I2S_PORT = I2S_NUM_0;
    static const int SAMPLE_RATE = 44100;
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_writer;
    audio_mixer_handle_t mixer;
    bool isPlaying;
    int currentVolume;
};

#endif
*/