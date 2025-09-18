#define AIC3204
#include <Arduino.h>
#include <pico-audio.h>
#include <Adafruit_NeoPixel.h>

// ================= PIFOSTI ENUMS =================
#define SCREEN_WIDTH_X 16
#define SCREEN_HEIGHT_Y 5
#define NUM_PIXEL SCREEN_WIDTH_X * 6
#define NUM_PIXEL_SCREEN SCREEN_WIDTH_X * SCREEN_HEIGHT_Y
#define NUM_PIXEL_KEYS SCREEN_WIDTH_X
#define NUM_BUTTONS 16
#define ENC_PUSH 16 // index to get the encoder push
#define ENC_A 17 // index to get the encoder A value
#define ENC_B 18 // index to get the encoder B value
#define ENC_PINS 3
#define ENC_A_PIN 32
#define ENC_B_PIN 33
#define GATE_IN_1_PIN 28
#define GATE_IN_2_PIN 29
#define GATE_OUT_1_PIN 19
#define GATE_OUT_2_PIN 38
#define MIDI_RX_PIN 17
#define MIDI_RX_UART uart0
#define MIDI_TX_TIP_PIN 21
#define MIDI_TX_RING_PIN 22

enum ADC_inputs
{
    CV_IN_1,
    CV_IN_2,
    POT_1,
    POT_2,
    NUM_ADC_CH
};

enum ButtonState
{
    UNCERTAIN,
    PRESSED,
    LONGPRESSED,
    RELEASED,
    DOWN,
    UP
};

enum Gate_in_list
{
    GATE_IN_1,
    GATE_IN_2,
    NUM_GATE_IN
};

enum Gate_out_list
{
    GATE_OUT_1,
    GATE_OUT_2,
    NUM_GATE_OUT
};

// Musical note constants for buttons
enum MusicalNote
{
    NOTE_OFF = 0,   // No note
    NOTE_C0 = 24,   // MIDI note 24
    NOTE_Cx0 = 25,  // C#0
    NOTE_D0 = 26,   // MIDI note 26
    NOTE_Dx0 = 27,  // D#0
    NOTE_E0 = 28,   // MIDI note 28
    NOTE_F0 = 29,   // MIDI note 29
    NOTE_Fx0 = 30,  // F#0
    NOTE_G0 = 31,   // MIDI note 31
    NOTE_Gx0 = 32,  // G#0
    NOTE_A0 = 33,   // MIDI note 33
    NOTE_Ax0 = 34,  // A#0
    NOTE_B0 = 35,   // MIDI note 35
    NOTE_C1 = 36    // MIDI note 36
};


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"


struct Button_Input
{
    uint8_t pin;
    uint8_t led;
    uint8_t note;
    uint8_t state;
};

struct Gate_Input
{
    uint8_t pin;
    uint8_t state;
};

Button_Input button[NUM_BUTTONS+1];
Gate_Input gate_in[NUM_GATE_IN];

uint8_t button_history[NUM_BUTTONS+3];
uint8_t gate_in_history[NUM_GATE_IN];

static int8_t Encoder_inc;

static inline void update_buttons() {
    uint8_t state;

    for (int i = 0; i < NUM_BUTTONS+1; i++)
    {
        state = UNCERTAIN;
        button_history[i] = (button_history[i] << 1) | !gpio_get(button[i].pin);

        if(button_history[i] == 1) {
             state = PRESSED;
        } else if(button_history[i] == (1 << 7)) {
            state = RELEASED;
        } else if(button_history[i] & 0x7f != 0) {
            state = DOWN;
        } else {
            state = UP;
        }

        button[i].state = state;
    }

    button_history[ENC_A] = (button_history[ENC_A] << 1) | !gpio_get(ENC_A_PIN);
    button_history[ENC_B] = (button_history[ENC_B] << 1) | !gpio_get(ENC_B_PIN);

    Encoder_inc = 0;
    if((button_history[ENC_A] & 0x03) == 0x02 && (button_history[ENC_B] & 0x03) == 0x00)
    {
        Encoder_inc = -1;
    }
    else if((button_history[ENC_B] & 0x03) == 0x02 && (button_history[ENC_A] & 0x03) == 0x00)
    {
        Encoder_inc = 1;
    }
}

static inline int8_t get_Encoder_inc() {
    return Encoder_inc;
}

static inline void update_gate_in() {
    for (int i = 0; i < NUM_GATE_IN; i++)
    {
        uint8_t state = UNCERTAIN;
        gate_in_history[i] = (gate_in_history[i] << 1) | !gpio_get(gate_in[i].pin);

        if(gate_in_history[i] == 1) {
             state = PRESSED;
        } else if(gate_in_history[i] == (1 << 7)) {
            state = RELEASED;
        } else if(gate_in_history[i] & 0x7f != 0) {
            state = DOWN;
        } else {
            state = UP;
        }

        gate_in[i].state = state;
    }
}

static inline void init_buttons() {
    button[0].pin = 35;
    button[0].note = NOTE_Cx0;

    button[1].pin = 11;
    button[1].note = NOTE_Dx0;

    button[2].pin = 13;
    button[2].note = NOTE_OFF;

    button[3].pin = 15;
    button[3].note = NOTE_Fx0;

    button[4].pin = 18;
    button[4].note = NOTE_Gx0;

    button[5].pin = 25;
    button[5].note = NOTE_Ax0;

    button[6].pin = 27;
    button[6].note = NOTE_OFF;

    button[7].pin = 31;
    button[7].note = NOTE_OFF;

    button[8].pin = 0;
    button[8].note = NOTE_C0;

    button[9].pin = 10;
    button[9].note = NOTE_D0;

    button[10].pin = 12;
    button[10].note = NOTE_E0;

    button[11].pin = 14;
    button[11].note = NOTE_F0;

    button[12].pin = 16;
    button[12].note = NOTE_G0;

    button[13].pin = 20;
    button[13].note = NOTE_A0;

    button[14].pin = 26;
    button[14].note = NOTE_B0;

    button[15].pin = 30;
    button[15].note = NOTE_C1;

    button[ENC_PUSH].pin = 34;
    button[ENC_PUSH].note = NOTE_OFF;

    for (int i = 0; i < NUM_BUTTONS+1; i++)
    {
        button[i].state = 0;
        button[i].led = NUM_PIXEL_SCREEN + i;
        gpio_init(button[i].pin);
        gpio_set_dir(button[i].pin, GPIO_IN);
        gpio_set_pulls(button[i].pin, false, false);
    }

    button[ENC_PUSH].led = NUM_PIXEL_SCREEN-1;

    gpio_init(ENC_A_PIN);
    gpio_set_dir(ENC_A_PIN, GPIO_IN);
    gpio_set_pulls(ENC_A_PIN, false, false);

    gpio_init(ENC_B_PIN);
    gpio_set_dir(ENC_B_PIN, GPIO_IN);
    gpio_set_pulls(ENC_B_PIN, false, false);

    update_buttons();
}

static inline void init_gate_in() {
    gate_in[0].pin = 28;
    gate_in[1].pin = 29;

    for (int i = 0; i < NUM_GATE_IN; i++)
    {
        gate_in[i].state = 0;
        gpio_init(gate_in[i].pin);
        gpio_set_dir(gate_in[i].pin, GPIO_IN);
        gpio_set_pulls(gate_in[i].pin, false, false);
    }

    update_gate_in();
}

#ifdef __cplusplus
}
#endif









// ================= CONFIG =================
#define NEOPIXEL_PIN      1
#define LED_COUNT         96
#define LED_UPDATE_MS     16
#define ENCODER_UPDATE_MS 5
#define DECAY_RATE        0.85f
#define SENSITIVITY       10.0f
#define REVERB_STEP       0.1f
#define WETDRY_STEP       0.02f
#define LED_MIN           0
#define LED_MAX           100
#define LED_GLOBAL_BRIGHTNESS 30  // 0-255

// ================= GLOBALS =================
Adafruit_NeoPixel pixels(LED_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Audio chain
AudioInputI2S input;
AudioOutputI2S output;
AudioSynthWaveformSine sine1;
AudioMixer4 mixer1, mixer2;
AudioAnalyzePeak peakOut;
AudioEffectEnvelope envelope1;
AudioEffectReverb reverb1;

// Audio connections
AudioConnection patchCord1(input,0,mixer1,0);
AudioConnection patchCord2(sine1,0,envelope1,0);
AudioConnection patchCord3(envelope1,0,mixer1,1);
AudioConnection patchCord4(mixer1,0,reverb1,0);
AudioConnection patchCord5(mixer1,0,mixer2,0);
AudioConnection patchCord6(reverb1,0,mixer2,1);
AudioConnection patchCord7(mixer2,0,output,0);
AudioConnection patchCord8(mixer2,0,output,1);
AudioConnection patchCord9(mixer2,peakOut);

// Shared variables
volatile float audioLevel = 0.0f;
volatile float smoothedLevel = 0.0f;
volatile float reverbTime = 3.5f;
volatile float wetDryMix = 0.5f;
volatile bool reverbTimeUpdated = false;
volatile bool wetDryMixUpdated = false;
volatile float volume = 0.5f;

// LED per mode
volatile uint8_t ledCountReverb = 50;
volatile uint8_t ledCountWetDry = 50;
volatile uint8_t controlMode = 2;

// ================= CORE 0 (Audio) =================
void setup() {
    Serial.begin(115200);
    init_buttons();

    AudioMemory(120);
    sine1.amplitude(0.3f);
    sine1.frequency(440.0f);

    mixer1.gain(0, 1.0f);
    mixer1.gain(1, 1.0f);
    mixer2.gain(0, volume * (1.0f - wetDryMix));
    mixer2.gain(1, volume * wetDryMix);

    reverb1.reverbTime(reverbTime);
    envelope1.attack(100);
    envelope1.decay(100);
    envelope1.release(500);
    envelope1.sustain(0.2f);

    output.begin(6, 7, 5);
    input.begin(7, 6, 4);
}

void loop() {
    update_buttons();

    // Capture audio level
    if (peakOut.available()) {
        float level = peakOut.read() * SENSITIVITY;
        audioLevel = constrain(level, 0.0f, 1.0f);
    }

    // Apply updated parameters
    if (reverbTimeUpdated) {
        reverb1.reverbTime(reverbTime);
        reverbTimeUpdated = false;
    }
    if (wetDryMixUpdated) {
        mixer2.gain(0, volume * (1.0f - wetDryMix));
        mixer2.gain(1, volume * wetDryMix);
        wetDryMixUpdated = false;
    }

    delay(1);
}

// ================= CORE 1 (UI) =================
void setup1() {
    pixels.begin();
    pixels.clear();
    pixels.show();
}

void loop1() {
    static unsigned long lastLED = 0;
    static unsigned long lastEnc = 0;
    static unsigned long lastPush = 0;

    update_buttons();

    // --- Encoder handling ---
    int8_t encInc = get_Encoder_inc();

    if (encInc != 0 && millis() - lastEnc > ENCODER_UPDATE_MS) {
        switch(controlMode) {
            case 0: // Reverb
                reverbTime = constrain(reverbTime + encInc * REVERB_STEP, 0.5f, 10.0f);
                reverbTimeUpdated = true;
                ledCountReverb = constrain((int)ledCountReverb + encInc, LED_MIN, LED_MAX);
                Serial.print("Reverb: "); Serial.print(reverbTime); Serial.print(" | LEDs: "); Serial.println(ledCountReverb);
                break;
            case 1: // Wet/Dry
                wetDryMix = constrain(wetDryMix + encInc * WETDRY_STEP, 0.0f, 1.0f);
                wetDryMixUpdated = true;
                ledCountWetDry = constrain((int)ledCountWetDry + encInc, LED_MIN, LED_MAX);
                Serial.print("Wet/Dry: "); Serial.print(wetDryMix); Serial.print(" | LEDs: "); Serial.println(ledCountWetDry);
                break;
            case 2: // VU meter: adjust volume
                volume = constrain(volume + encInc * 0.05f, 0.0f, 1.0f);
                mixer2.gain(0, volume * (1.0f - wetDryMix));
                mixer2.gain(1, volume * wetDryMix);
                Serial.print("Volume: "); Serial.println((int)(volume*100));
                break;
        }
        lastEnc = millis();
    }

    // --- Encoder push toggle ---
    if (button[ENC_PUSH].state == PRESSED && millis() - lastPush > 200) {
        controlMode = (controlMode + 1) % 3; // cycle modes
        Serial.print("Mode: ");
        if (controlMode == 0) Serial.println("Reverb");
        else if (controlMode == 1) Serial.println("Wet/Dry");
        else Serial.println("VU Meter");
        lastPush = millis();
    }

    // --- Note button ---
    if (button[8].state == PRESSED) envelope1.noteOn();
    else if (button[8].state == RELEASED) envelope1.noteOff();

    // --- LED display ---
    if (millis() - lastLED >= LED_UPDATE_MS) {
        smoothedLevel = smoothedLevel * DECAY_RATE + audioLevel * (1.0f - DECAY_RATE);

        int litPixels = 0;
        if (controlMode == 0) litPixels = ledCountReverb;
        else if (controlMode == 1) litPixels = ledCountWetDry;
        else litPixels = constrain((int)(smoothedLevel * LED_COUNT), 0, LED_COUNT);

        float factor = LED_GLOBAL_BRIGHTNESS / 255.0f;

        for (int i = 0; i < LED_COUNT; i++) {
            if (i < litPixels) {
                uint8_t r = (controlMode==2) ? (128 + (127*i)/LED_COUNT) : 0;
                uint8_t g = (controlMode==1) ? 255 : (controlMode==2 ? (100*i)/LED_COUNT : 100);
                uint8_t b = (controlMode==0) ? 255 : (controlMode==2 ? 255 : 100);

                r = r * factor;
                g = g * factor;
                b = b * factor;

                pixels.setPixelColor(i, pixels.Color(r, g, b));
            } else {
                pixels.setPixelColor(i, 0);
            }
        }

        pixels.show();
        lastLED = millis();
    }
}
