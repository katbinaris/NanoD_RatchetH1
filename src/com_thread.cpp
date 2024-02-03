#include "com_thread.h"
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, midi2)


// Very primitive function sending MIDI note On/Off via UART

ComThread::ComThread(const uint8_t task_core) : Thread("COM", 2048, 1, task_core) {}

ComThread::~ComThread() {}

void ComThread::run() {
    
    Serial2.begin(31250, SERIAL_8N1, 43, 44);
    midi2.begin(MIDI_CHANNEL_OMNI);
    
    while (1) {
      midi2.sendNoteOn(60, 127, 1);
      delay(100);
      midi2.sendNoteOff(60,0,1);
      delay(100);
    }
}
