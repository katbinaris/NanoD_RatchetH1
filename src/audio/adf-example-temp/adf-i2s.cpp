/*
#include "adf-i2s.h"

MyI2S::MyI2S() : isPlaying(false), currentVolume(50) { // Default volume 50%
    // Constructor implementation
}

void MyI2S::begin() {
    configureI2S();

    // Initialize ADF pipeline
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    // Create i2s stream writer element
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    // Create mixer element
    audio_mixer_cfg_t mixer_cfg = DEFAULT_AUDIO_MIXER_CONFIG();
    mixer_cfg.resample_rate = SAMPLE_RATE;
    mixer = audio_mixer_init(&mixer_cfg);

    // Register elements to pipeline
    audio_pipeline_register(pipeline, mixer, "mixer");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    const char *link_tag[2] = {"mixer", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    audio_pipeline_run(pipeline);
}

void MyI2S::configureI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = 0, // Default interrupt priority
        .dma_buf_count = 8,
        .dma_buf_len = 64,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
}

void MyI2S::playSample(const uint8_t *sample, size_t size) {
    // Create a ring buffer for the sample
    audio_element_handle_t sample_stream = rb_init("sample_rb", 8, 64, NULL, NULL, false);
    audio_element_set_write_cb(sample_stream, sample, size);

    // Add the sample stream to the mixer
    audio_mixer_ch_open(mixer, sample_stream);

    // Indicate that the sample is playing
    isPlaying = true;
}

void MyI2S::setVolume(int volume) {
    currentVolume = volume;
    if (mixer != nullptr) {
        audio_mixer_set_volume(mixer, volume);
    }
}

void MyI2S::handleAudio() {
    if (isPlaying) {
        audio_pipeline_wait_for_stop(pipeline);
        isPlaying = false;
    }
}
*/