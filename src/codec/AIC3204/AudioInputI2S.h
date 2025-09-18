#ifndef AudioInputI2S_h
#define AudioInputI2S_h

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include "AudioStream.h"

extern "C" {
    #include "hardware/pio.h"
    #include "hardware/dma.h"
    #include "hardware/i2c.h"
    #include "hardware/clocks.h"
    
    void init_codec();
    void init_audio_pio();
    extern volatile int16_t rx_audio_buffer_0[];
    extern volatile int16_t rx_audio_buffer_1[];
    extern int rx_last_buffer;
    extern PIO pio_aic3204;
    extern unsigned int sm_aic3204;
    extern unsigned int offset_aic3204;
}

class AudioInputI2S : public AudioStream {
public:
    audio_block_t *inputQueueArray[0];

private:
    inline static audio_block_t *block_left = nullptr;
    inline static audio_block_t *block_right = nullptr;
    inline static uint16_t block_offset = 0;
    inline static bool update_responsibility = false;
    inline static volatile bool new_data_available = false;
    inline static volatile bool codec_initialized = false;

public:
    // Constructor
    AudioInputI2S() : AudioStream(0, inputQueueArray) {}

    // Begin method
    inline void begin(unsigned int pBCLK = 7, unsigned int pWS = 6, unsigned int pDATA_IN = 4) {
        block_left = nullptr;
        block_right = nullptr;
        block_offset = 0;
        new_data_available = false;
        codec_initialized = true;
        update_responsibility = update_setup();
    }

    // Process audio callback from DMA/C library
    inline static void process_audio_callback(int16_t *buf) {
        static uint32_t last_process_time = 0;
        uint32_t now = micros();
        new_data_available = true;
    }

    // Update function called by AudioStream
    inline void update() override {
        audio_block_t *new_left = allocate();
        audio_block_t *new_right = allocate();

        __disable_irq();

        if (new_data_available) {
            volatile int16_t *source_buffer = (rx_last_buffer == 0) ? rx_audio_buffer_1 : rx_audio_buffer_0;

            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                new_left->data[i] = source_buffer[i * 2];
                new_right->data[i] = source_buffer[i * 2 + 1];
            }

            new_data_available = false;
            __enable_irq();

            transmit(new_left, 0);
            transmit(new_right, 1);

            release(new_left);
            release(new_right);
        } else {
            __enable_irq();
            release(new_left);
            release(new_right);
        }
    }

    // Helper to check if new data is available
    inline static bool hasNewData() { return new_data_available; }
};

#endif
