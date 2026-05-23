#define AIC3204
#include <Arduino.h>
#include <pico-audio.h>

// ================= GLOBALS =================

// Audio chain
AudioInputI2S input;
AudioOutputI2S output;
AudioSynthWaveformSine sine1;
AudioMixer4 mixer1, mixer2;
AudioEffectReverb reverb1;

// Audio connections
AudioConnection patchCord1(input, 0, mixer1, 0);    // Input to mixer1
AudioConnection patchCord2(sine1, 0, mixer1, 1);    // Sine to mixer1
AudioConnection patchCord3(mixer1, 0, reverb1, 0);  // Mixer1 to reverb
AudioConnection patchCord4(mixer1, 0, mixer2, 0);   // Mixer1 to mixer2 (dry)
AudioConnection patchCord5(reverb1, 0, mixer2, 1);  // Reverb to mixer2 (wet)
AudioConnection patchCord6(mixer2, 0, output, 0);   // Mixer2 to left output
AudioConnection patchCord7(mixer2, 0, output, 1);   // Mixer2 to right output

// ================= CORE 0 (Audio) =================
void setup() {
    Serial.begin(115200);
    AudioMemory(120);

    sine1.amplitude(0.3f);
    sine1.frequency(440.0f);
    
    mixer1.gain(0, 0.8f); // Input gain
    mixer1.gain(1, 0.5f); // Sine wave gain
    
    reverb1.reverbTime(3.5f); // 3.5 second reverb tail
    
    mixer2.gain(0, 0.6f); // Dry signal (60%)
    mixer2.gain(1, 0.4f); // Wet signal (40%)
    
    // Start audio I/O
    output.begin(6, 7, 5);
    input.begin(7, 6, 4);
    
    Serial.println("Audio started - Input + Sine -> Reverb -> Output");
}

void loop() {
    delay(10);
}