#include "AudioOutputI2S.h"

// Define the static I2S instance
I2S AudioOutputI2S::i2s(OUTPUT);

AudioOutputI2S::AudioOutputI2S()
  : AudioStream(2, inputQueueArray) {
}

extern void I2S_Transmitted(void);

void AudioOutputI2S::begin(uint pBCLK, uint pWS, uint pDOUT) {
  // Configure I2S clock, data, bit-depth and frequency
  i2s.setBCLK(pBCLK);
  i2s.setDATA(pDOUT);
  i2s.setBitsPerSample(32);
  i2s.setFrequency(AUDIO_SAMPLE_RATE);
  // Sized for 6 blocks of stereo words
  i2s.setBuffers(6, AUDIO_BLOCK_SAMPLES * 2);
  
  // Set DMA completion callback to trigger audio updates
  i2s.onTransmit(I2S_Transmitted);

  if (!i2s.begin()) {
    Serial.println("Failed to initialize I2S!");
    while (1)
      ;  // Halt on hardware init failure
  }
  update_setup();
}

void AudioOutputI2S::update() {
  audio_block_t *inputLeftBlock = receiveReadOnly(0);
  audio_block_t *inputRightBlock = receiveReadOnly(1);
  static int32_t tmp[AUDIO_BLOCK_SAMPLES * 2];

  // Optimized sample packing: NULL checks hoisted outside the inner loop,
  // fast shift instead of 32-bit multiplication, and branchless 0-guard
  // for DAC auto-mute workarounds (e.g. PCM5100A/PCM5102).
  if (inputLeftBlock && inputRightBlock) {
    const int16_t *srcL = inputLeftBlock->data;
    const int16_t *srcR = inputRightBlock->data;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int32_t sL = srcL[i];
      int32_t sR = srcR[i];
      if (sL == 0) sL = 1;
      if (sR == 0) sR = 1;
      tmp[i * 2]     = sL << 16;
      tmp[i * 2 + 1] = sR << 16;
    }
  } else if (inputLeftBlock) {
    const int16_t *srcL = inputLeftBlock->data;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int32_t sL = srcL[i];
      if (sL == 0) sL = 1;
      tmp[i * 2]     = sL << 16;
      tmp[i * 2 + 1] = 1 << 16;
    }
  } else if (inputRightBlock) {
    const int16_t *srcR = inputRightBlock->data;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int32_t sR = srcR[i];
      if (sR == 0) sR = 1;
      tmp[i * 2]     = 1 << 16;
      tmp[i * 2 + 1] = sR << 16;
    }
  } else {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES * 2; i++) {
      tmp[i] = 1 << 16;
    }
  }
  
  i2s.write((uint8_t*)tmp, sizeof(tmp));
  
  if (inputLeftBlock) {
    release(inputLeftBlock);
  }
  if (inputRightBlock) {
    release(inputRightBlock);
  }
}
