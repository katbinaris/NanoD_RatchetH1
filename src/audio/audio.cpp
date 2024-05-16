
#include "audio.h"
#include "nanofoc_d.h"
#include "../haptic.h"

#ifdef USE_AUDIO_LIB
#include "XT_I2S_Audio.h"
XT_I2S_Class* xt_player = nullptr;
XT_PlayListItem_Class* clack_wav_sample = nullptr;
XT_PlayListItem_Class* loud_wav_sample = nullptr;
XT_PlayListItem_Class* soft_wav_sample = nullptr;
XT_PlayListItem_Class* hard_wav_sample = nullptr;
XT_PlayListItem_Class* chime_wav_sample = nullptr;
#endif
#ifndef USE_AUDIO_LIB
#define SAMPLES_PER_SEC 22050
#endif
#define DEFAULT_VOLUME 100


BinarisAudioPlayer audioPlayer; // global instance

uint8_t* get_audio_file(String fName) {
    if (fName=="loud")
        return loud_wav;
    else if (fName=="soft")
        return soft_wav;
    else if (fName=="hard")
        return hard_wav;
    else if (fName=="clack")
        return clack_wav;
    else if (fName=="chime")
        return chime_wav;
    return nullptr;  
};




String get_audio_filename(uint8_t* audio_file){
    if (audio_file==loud_wav)
        return "loud";
    else if (audio_file==soft_wav)
        return "soft";
    else if (audio_file==hard_wav)
        return "hard";
    else if (audio_file==clack_wav)
        return "clack";
    else if (audio_file==chime_wav)
        return "chime";
    return "none";
};


BinarisAudioPlayer::BinarisAudioPlayer(){
    _q_audio_in = xQueueCreate(2, sizeof( AudioCommand ));
    assert(_q_audio_in != NULL);
};

BinarisAudioPlayer::~BinarisAudioPlayer(){};



void BinarisAudioPlayer::audio_init(){

    check_file(String("hard.wav"), hard_wav);
    check_file(String("soft.wav"), soft_wav);
    check_file(String("clack.wav"), clack_wav);
    check_file(String("loud.wav"), loud_wav);
    check_file(String("chime.wav"), chime_wav);

    audio_config.audio_feedback_lvl = 100;
    audio_config.audio_file = hard_wav;

#ifdef USE_AUDIO_LIB
    xt_player = new XT_I2S_Class(PIN_I2S_LRC, PIN_I2S_BCLK, PIN_I2S_DOUT, I2S_NUM_0);
    xt_player->Volume = audio_config.audio_feedback_lvl;
    clack_wav_sample = new XT_Wav_Class((const unsigned char *)clack_wav);
    loud_wav_sample = new XT_Wav_Class((const unsigned char *)loud_wav);
    soft_wav_sample = new XT_Wav_Class((const unsigned char *)soft_wav);
    hard_wav_sample = new XT_Wav_Class((const unsigned char *)hard_wav);
    chime_wav_sample = new XT_Wav_Class((const unsigned char *)chime_wav);
    data_ptr=nullptr; // not used with xt_player
    Serial.println("XT Audio library initialized");
#else
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = SAMPLES_PER_SEC;                       // Note, max sample rate
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    #if defined(AUDIO_FILES_STEREO)
    i2s_config.chan_mask = I2S_CHANNEL_STEREO;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    #else
    i2s_config.chan_mask = I2S_CHANNEL_MONO;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ALL_LEFT;
    #endif
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;             // high interrupt priority
    i2s_config.dma_buf_count = 8;                                   // 8 buffers
    i2s_config.dma_buf_len = 256;                                   // 256 bytes per buffer, so 2K of buffer space
    i2s_config.use_apll=0;
    i2s_config.tx_desc_auto_clear= true; 
    i2s_config.fixed_mclk=-1;

	// Set up I2S pin config structure
 	pin_config.bck_io_num = PIN_I2S_BCLK;
    pin_config.ws_io_num = PIN_I2S_LRC;
    pin_config.data_out_num = PIN_I2S_DOUT;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_set_sample_rates(I2S_NUM_0, SAMPLES_PER_SEC);
#endif
};



// call from any thread
void BinarisAudioPlayer::play_audio(uint8_t* audio_file, uint16_t volume){
    if (audio_file!=nullptr) {
        AudioCommand command{ .type = AudioCommandType::PLAY_WAV, .audio_file = audio_file };
        xQueueSend(_q_audio_in, &command, (TickType_t)0);
    }
};



void BinarisAudioPlayer::put_audio_config(audioConfig& config){
    AudioCommand command{ .type = AudioCommandType::CONFIG, .config = config };
    xQueueSend(_q_audio_in, &command, (TickType_t)0);
};



void BinarisAudioPlayer::play_haptic_audio(){
    AudioCommand command{ .type = AudioCommandType::PLAY_HAPTIC };
    xQueueSend(_q_audio_in, &command, (TickType_t)0);
};



void BinarisAudioPlayer::start_play(uint8_t* audio_file){
    #ifdef USE_AUDIO_LIB
        XT_PlayListItem_Class* sample;
        if (audio_file==clack_wav)
            sample = clack_wav_sample;
        else if (audio_file==loud_wav)
            sample = loud_wav_sample;
        else if (audio_file==soft_wav)
            sample = soft_wav_sample;
        else if (audio_file==hard_wav)
            sample = hard_wav_sample;
        else if (audio_file==chime_wav)
            sample = chime_wav_sample;
        else 
            sample = nullptr;
        if (sample==nullptr)
            Serial.println("? audio file");
        if (xt_player!=nullptr)
            Serial.println("? audio plr");
        if (sample!=nullptr && xt_player!=nullptr) {
            xt_player->Play(sample);
            Serial.println("Playing "+get_audio_filename(audio_file));
        }
    #else
        data_ptr = &audio_file[44];
        num_bytes_remaining = audio_file[40] + (audio_file[41] << 8) + (audio_file[42] << 16) + (audio_file[43] << 24);
    #endif
};


bool BinarisAudioPlayer::check_file(String fName, uint8_t* audio_file){
    if (!(audio_file[0] == 'R' && audio_file[1] == 'I' && audio_file[2] == 'F' && audio_file[3] == 'F' &&
        audio_file[8] == 'W' && audio_file[9] == 'A' && audio_file[10] == 'V' && audio_file[11] == 'E' &&
        audio_file[12] == 'f' && audio_file[13] == 'm' && audio_file[14] == 't' && audio_file[15] == ' ' &&
        audio_file[36] == 'd' && audio_file[37] == 'a' && audio_file[38] == 't' && audio_file[39] == 'a')) {
            Serial.println(fName + ": audio file is not a valid WAV file.");
        return false;
    }
    #if defined(AUDIO_FILES_STEREO)
    if (!(audio_file[20]==0x01 && audio_file[21]==0x00 && audio_file[22]==0x02 && audio_file[23]==0x00)) {
        Serial.println(fName + ": audio file is not 2 channel PCM.");
        return false;
    }
    #else
    if (!(audio_file[20]==0x01 && audio_file[21]==0x00 && audio_file[22]==0x01 && audio_file[23]==0x00)) {
        Serial.println(fName + ": audio file is not 1 channel PCM.");
        return false;
    }
    #endif
    if (!(audio_file[24]==0x22 && audio_file[25]==0x56 && audio_file[26]==0x00 && audio_file[27]==0x00)) {
        Serial.println(fName + ": audio file is not 22kHz.");
        return false;
    }
    if (!(audio_file[32]==0x04 && audio_file[33]==0x00)) {
        Serial.println(fName + ": block align is not 4.");
        return false;
    }
    if (!(audio_file[34]==0x10 && audio_file[35]==0x00)) {
        Serial.println(fName + ": audio file is not 16 bit.");
        return false;
    }
    uint32_t data_size = audio_file[40] + (audio_file[41] << 8) + (audio_file[42] << 16) + (audio_file[43] << 24);
    if (data_size%4 != 0) {
        Serial.println(fName + ": audio data size is not a multiple of 4.");
        return false;
    }
    return true;
};


void BinarisAudioPlayer::handle_audio_commands(){
    AudioCommand command;
    if (xQueueReceive(_q_audio_in, &command, (TickType_t)0) == pdTRUE){
        switch (command.type){
            case AudioCommandType::PLAY_WAV:
                if (data_ptr==nullptr) { // only play if no audio is currently playing
                    start_play(command.audio_file);
                }
                else
                    Serial.println("x");
                break;
            case AudioCommandType::PLAY_HAPTIC:
                if (data_ptr==nullptr) { // only play if no audio is currently playing
                    start_play(audio_config.audio_file);
                }
                else
                    Serial.println("x");
                break;
            case AudioCommandType::CONFIG:
                //Serial.println("Audio config");
                audio_config = command.config;
                #ifdef USE_AUDIO_LIB
                if (xt_player!=nullptr)
                    xt_player->Volume = audio_config.audio_feedback_lvl;
                #endif
                break;
            default:
                break;
        }
    }

};


int iter = 0;

// always call from one thread
void BinarisAudioPlayer::audio_loop(){
    // fill the audio buffer with the next audio sample
    handle_audio_commands();
#ifdef USE_AUDIO_LIB
    if (xt_player!=nullptr) {
        xt_player->Volume = 125;
        xt_player->FillBuffer();
    }
#else
    if (data_ptr==nullptr) {
        return;
    }
    size_t num_bytes_written = 0;
    esp_err_t err = i2s_write(I2S_NUM_0, data_ptr, num_bytes_remaining, &num_bytes_written, 0);
    num_bytes_remaining = num_bytes_remaining - num_bytes_written;
    if (num_bytes_remaining == 0){
        // we have finished playing the audio file
        data_ptr = nullptr;
        //Serial.println("Audio done in "+String(iter));
        iter = 0;
    }
    else {
        iter++;
        data_ptr = data_ptr + num_bytes_written;
    }
#endif
};


extern "C" void HapticInterface::UserHapticEventCallback(HapticEvt event, float currentAngle, uint16_t currentPos){
  switch (event) {
    case HapticEvt::INCREASE:
        audioPlayer.play_haptic_audio();
        break;
    case HapticEvt::DECREASE:
        audioPlayer.play_haptic_audio();
        break;
    default:
        break;
  }
};