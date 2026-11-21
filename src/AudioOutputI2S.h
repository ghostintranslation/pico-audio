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
  void begin(uint pBCLK = 20, uint pWS = 21, uint pDOUT = 22);
  void update();
};

#endif // AudioOutputI2S_h