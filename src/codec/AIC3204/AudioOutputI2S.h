#ifndef AudioOutputI2S_h
#define AudioOutputI2S_h

#include <Arduino.h>
#include <cstdint>
#include "AudioStream.h"

extern "C" {
    #include "hardware/pio.h"
    #include "hardware/dma.h"
    #include "hardware/i2c.h"
    #include "hardware/clocks.h"
    
    void init_codec();
    void init_audio_pio();
    extern volatile int16_t tx_audio_buffer_0[];
    extern volatile int16_t tx_audio_buffer_1[];
    extern int tx_last_buffer;
}

class AudioOutputI2S : public AudioStream {
public:
    audio_block_t *inputQueueArray[2];

private:
    static volatile bool codec_initialized;
    static volatile bool generate_callback_registered;
    
    // Static instance pointer for callback access
    static AudioOutputI2S* callback_instance;

public:
    AudioOutputI2S();
    void begin(unsigned int pBCLK = 6, unsigned int pWS = 7, unsigned int pDOUT = 5);
    virtual void update();    
    // Generate audio callback for C library DMA handler
    static void generate_audio_callback(int16_t *buf);
};

// Forward declaration - this is a C++ function from AudioStream
void I2S_Transmitted(void); // Trigger update_all()

// Static member definitions for AudioOutputI2S
volatile bool AudioOutputI2S::codec_initialized = false;
volatile bool AudioOutputI2S::generate_callback_registered = false;

// Static instance pointer for callback access
AudioOutputI2S* AudioOutputI2S::callback_instance = nullptr;

AudioOutputI2S::AudioOutputI2S() : AudioStream(2, inputQueueArray) {
    // Store instance for callback access
    callback_instance = this;
}

void AudioOutputI2S::begin(unsigned int pBCLK, unsigned int pWS, unsigned int pDOUT) {
    // Initialize the codec and PIO I2S using C library
    init_codec();
    delay(100); // Allow codec to stabilize
    
    // Reset statistics
    codec_initialized = true;
    generate_callback_registered = true;
    
    // Set up update responsibility
    update_setup();
}

void AudioOutputI2S::generate_audio_callback(int16_t *buf) {
    static uint32_t last_generate_time = 0;
    uint32_t now = micros();
    
    Serial.print("Callback called at: ");
    Serial.println(now);
    
    if (!codec_initialized || !generate_callback_registered || !callback_instance) {
        Serial.println("Error: Codec not initialized or callback not registered!");
        memset(buf, 0, AUDIO_BLOCK_SAMPLES * 2 * sizeof(int16_t));
        return;
    }
    
    AudioStream::update_all();
    
    audio_block_t *block_left = callback_instance->receiveReadOnly(0);
    audio_block_t *block_right = callback_instance->receiveReadOnly(1);
    
    if (block_left && block_right) {
        Serial.println("Processing audio blocks...");
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            buf[i * 2] = block_left->data[i];
            buf[i * 2 + 1] = block_right->data[i];
            if (i < 5) { // Print first few samples for debugging
                Serial.print("L: "); Serial.print(buf[i * 2]);
                Serial.print(" R: "); Serial.println(buf[i * 2 + 1]);
            }
        }
        AudioStream::release(block_left);
        AudioStream::release(block_right);
    } else {
        Serial.println("No audio blocks available!");
        memset(buf, 0, AUDIO_BLOCK_SAMPLES * 2 * sizeof(int16_t));
        if (block_left) AudioStream::release(block_left);
        if (block_right) AudioStream::release(block_right);
    }
    
    last_generate_time = now;
}
void AudioOutputI2S::update() {
    // For output, the actual work is done in the generate_audio_callback
    // This method just needs to handle any additional processing
}

#endif