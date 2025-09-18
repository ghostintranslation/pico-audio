#ifndef AudioOutputI2S_h
#define AudioOutputI2S_h

#include <I2S.h>

#include "AudioStream.h"

class AudioOutputI2S : public AudioStream {
protected:
  audio_block_t *inputQueueArray[2];

public:
  static I2S i2s;
  AudioOutputI2S();
  void begin(uint pBCLK, uint pWS, uint pDOUT);
  void update();
};

I2S AudioOutputI2S::i2s(OUTPUT);

inline AudioOutputI2S::AudioOutputI2S()
  : AudioStream(2, inputQueueArray) {
}

extern void I2S_Transmitted(void);
inline void AudioOutputI2S::begin(uint pBCLK = 20, uint pWS = 21, uint pDOUT = 22) {

  // ********** I2S **********
  i2s.setBCLK(pBCLK);
  //i2s.setMCLK(pWS);
  i2s.setDATA(pDOUT);
  i2s.setBitsPerSample(32);
  i2s.setFrequency(AUDIO_SAMPLE_RATE);
  i2s.setBuffers(6, AUDIO_BLOCK_SAMPLES * 2 * sizeof(int32_t) / sizeof(uint32_t));
  
// pinMode(15,OUTPUT);  
  i2s.onTransmit(I2S_Transmitted);

  // start I2S at the sample rate with 16-bits per sample
  if (!i2s.begin()) {
    Serial.println("Failed to initialize I2S!");
    while (1)
      ;  // do nothing
  }
  update_setup();
}


inline void AudioOutputI2S::update() {
  audio_block_t *inputLeftBlock = receiveReadOnly(0);
  audio_block_t *inputRightBlock = receiveReadOnly(1);
	static int32_t tmp[AUDIO_BLOCK_SAMPLES*2]; // 4*256*2 = 2kB; saves a lot of CPU, though!

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    // When sending 0, some DAC will power off causing a hearable jump from silence to floor noise and cracks,
    // sending 1 is a workaround
    // Tested only with the PCM5100A
    int16_t sampleL = inputLeftBlock == NULL || inputLeftBlock->data[i] == 0 ? 1 : inputLeftBlock->data[i];// * 0.1;
    int16_t sampleR = inputRightBlock == NULL || inputRightBlock->data[i] == 0 ? 1 : inputRightBlock->data[i];// * 0.1;

	// fill the temporary buffer - MUCH faster than
	// writing samples two at a time
	tmp[i*2]   = (int32_t)sampleL*65536;
	tmp[i*2+1] = (int32_t)sampleR*65536;
  }
  
  // doesn't fail on overrun, but what would we do if 
  // a check showed we didn't write all data?!
  i2s.write((uint8_t*) tmp, sizeof tmp);
  
  if (inputLeftBlock) {
    release(inputLeftBlock);
  }
  if (inputRightBlock) {
    release(inputRightBlock);
  }
}

#endif