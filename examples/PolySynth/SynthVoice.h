#ifndef SynthVoice_h
#define SynthVoice_h

#include <pico-audio.h>

class SynthVoice {
private:
AudioSynthWaveformSine* sine;
AudioEffectEnvelope* envelope;
uint8_t note;

public:
  SynthVoice();
  void noteOn();
  void noteOff();
  void noteOn(uint8_t note);
  void setNote(uint8_t note);
  uint8_t getNote();
  AudioStream* getOutput();
};

inline SynthVoice::SynthVoice() {
  this->sine = new AudioSynthWaveformSine();
  this->sine->amplitude(1.0);
  this->envelope = new AudioEffectEnvelope();
  this->envelope->attack(5);
  // this->envelope->decay(2000);
  this->envelope->sustain(1);
  this->envelope->release(4000);

  new AudioConnection(*this->sine, 0, *this->envelope, 0);
}

inline void SynthVoice::noteOn(uint8_t note) {
  this->setNote(note);
  this->envelope->noteOn();
}

inline void SynthVoice::noteOn() {
  this->envelope->noteOn();
}

inline void SynthVoice::noteOff() {
  this->envelope->noteOff();
}

inline void SynthVoice::setNote(uint8_t note) {
  this->note = note;
  this->sine->frequency(440.0f * exp2f((note - 69) * (1.0f / 12.0f)));
}

inline uint8_t SynthVoice::getNote() {
  return this->note;
}

inline AudioStream* SynthVoice::getOutput() {
  return this->envelope;
}

#endif