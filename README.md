# pico-audio

PJRC Teensy Audio library ported to Pico 2.

Requires RP2350 (Pico 2, Pico 2 W or compatible boards) - will NOT run on RP2040. RP2350 has DSP extensions which are required for this library to work correctly.

Most of the Teensy Audio functions have been ported. While not extensively tested seems to work as it does on Teensy 3.2 and Teensy 4. Functions that have not been ported are for the most part specific to Teensy hardware. Teensy sketches will require modifications to define the RP2350 I2S pins in use and likely other modifications to Teensy specific code. DSP intensive code may require overclocking the RP2350 - see comments about perfmance below.

Note that Teensy 4 and 4.1 are MUCH faster that the RP2350 (approx 5x) so if you need high performance DSP use Teensy 4.

This fork has been modifed from the original 96k/256 sample port to use 44.1k samples per second and 128 sample blocks to maximize compatibilty with Teensy code. Lower samplerate=lower CPU usage.

The library currently supports 16 bit I2S audio output only and does not yet support audio input.

Library has two added modules: Synth_DaisySP and Effect_DaisySP which allow usage of Electrosmith's DaisySP DSP library with Teensy Audio. 
https://github.com/electro-smith/DaisySP
https://github.com/rheslip/DaisySP_Teensy


Tested with Raspberry Pi Pico 2 board, Arduino IDE 2.3.6 with Pico Arduino 4.5.3 and a PCM5102 I2S DAC module.

To run the PolySynth example, select the Pico 2 board and USB stack Adafruit TinyUSB. Ladderfilter example may require overclocking. Most of the original Teensy Audio examples will run with minor modifications.

Known Issues:

Sometimes memory usage reports wildly wrong numbers

### New Features:
    - Multi codec AIC3204/PCM5102
    - Input and Output
    - Codec examples
    - Platformio to test Arduino Examples

### TODO:
    - SGLT5000
    - analyze_fft256

### Notes:
    - Had to reduce to 16000/64 SAMPLE_RATE/BLOCK_SAMPLES for audioInput/audioOutput to work.
    - Move SAMPLE_RATE/BLOCK_SAMPLES to enums.h unify into a single file and share it with codec files.