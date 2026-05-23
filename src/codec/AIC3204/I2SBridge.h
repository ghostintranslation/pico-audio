
#ifndef AudioI2SBridge_h
#define AudioI2SBridge_h

#include <Arduino.h>
#include <cstdint>
#include "AudioInputI2S.h"
#include "AudioOutputI2S.h"

extern "C" {
    #include "aic3204_i2s.pio.h"
    #include "aic3204.h"

    // Called by the C library's DMA interrupt handler when audio input data is ready
    void process_audio(int16_t *buf) {
        AudioInputI2S::process_audio_callback(buf);
    }
    
    // Called by the C library's DMA interrupt handler when it needs audio output data
    void generate_audio(int16_t *buf) {
        AudioOutputI2S::generate_audio_callback(buf);
    }
}

#endif
