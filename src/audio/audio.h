
#pragma once

#include <inttypes.h>
#include <driver/i2s.h>
#include "./audio_api.h"
#include <Arduino.h>

typedef enum {
    NONE = 0x00,
    CONFIG = 0x01,
    PLAY_HAPTIC = 0x02,
    PLAY_WAV = 0x03
} AudioCommandType;


struct AudioCommand {
    AudioCommandType type;
    union {
        audioConfig config;
        uint8_t* audio_file;
    };
};

class BinarisAudioPlayer {
    friend class HmiThreadButtonHandler;
    friend class HmiThread;
public:
    BinarisAudioPlayer();
    ~BinarisAudioPlayer();
    void audio_init();
    void play_audio(uint8_t* audio_file, uint16_t volume);
    void put_audio_config(audioConfig& config);
    void play_haptic_audio();
    void audio_loop();
    bool check_file(String fName, uint8_t* audio_file);
protected:
    void handle_audio_commands();
    void start_play(uint8_t* audio_file);

    i2s_driver_config_t i2s_config;
    i2s_pin_config_t pin_config;

    QueueHandle_t _q_audio_in;
    audioConfig audio_config; // haptic audio config

    size_t num_bytes_remaining = 0;
    uint8_t* data_ptr = nullptr;
};


extern BinarisAudioPlayer audioPlayer;
