
#pragma once

#include <inttypes.h>
#include <Arduino.h>

typedef struct {


    uint8_t audio_feedback_lvl;

    /*
        Audio File (UI: Audio Magnitude) set pre-uploaded audio file which can be stored as PROGMEM or in SPIFS
        Audio files are very small, in most of cases way under 500ms, stored as mono 16bit 24Khz samples, 
        Audio Files: 'faint' [0], 'soft' [1], 'default' [2], 'medium' [3], 'hard' [4],
        Note: It is not intended or for user to be able to upload or tune these pre-defined sound files. 
        Some documentation on sound design will be provided in User Documentation, 
        this however could be modified only by advanced users who want to play with source.

        TODO: In future to consider generative sound approach with virtual oscillator (sine, sqare, saw)?

    */

    /* 
        Toggles between USB (slave) or MIDI (standalone) operation, 
        In UI -> Profile Settings -> Connection Type
        If is_midi = 1, midi_thread is enabled and listens for 
        buttons or knob state update that contains CC message, else midi_thread is disabled

    */
   uint8_t* audio_file; // valid values are 'loud', 'soft', 'none', 'hard', 'clack'

   uint8_t* key_audio_file; // valid values are 'loud', 'soft', 'none', 'hard', 'clack'

} audioConfig;


extern uint8_t soft_wav[];
extern uint8_t hard_wav[];
extern uint8_t loud_wav[];
extern uint8_t clack_wav[];
extern uint8_t ping_wav[];

uint8_t* get_audio_file(String fName);
String get_audio_filename(uint8_t* audio_file);