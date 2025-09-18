#include <Adafruit_TinyUSB.h>
#define PCM5102
#include <pico-audio.h>
#include <pico-audio.h>

// GUItool: begin automatically generated code
AudioSynthWaveformSine   sine1;          //xy=204.3333282470703,195.3333282470703
AudioOutputI2S           output;           //xy=457.3333282470703,194.3333282470703
AudioConnection          patchCord1(sine1, 0, output, 0);
AudioConnection          patchCord2(sine1, 0, output, 1);
// GUItool: end automatically generated code

void setup() {
AudioMemory(200); //copy paste del ejemplo
sine1.amplitude(1);
sine1.frequency(440);
output.begin(0,1,2);

}

void loop() {
  // put your main code here, to run repeatedly:

}