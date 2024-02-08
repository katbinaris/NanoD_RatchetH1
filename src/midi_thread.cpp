
#include "./midi_thread.h"


MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, midi2)

MidiThread::MidiThread(const uint8_t task_core) : Thread("MIDI", 2048, 1, task_core) {

};

MidiThread::~MidiThread() {
    // nothing to do
}

void MidiThread::run() {
    
    Serial2.begin(31250, SERIAL_8N1, 43, 44);
    midi2.begin(MIDI_CHANNEL_OMNI);
    
    while (true) {
        midi2.sendNoteOn(60, 127, 1);
        delay(100);
        midi2.sendNoteOff(60,0,1);
        delay(100);
    }
}
