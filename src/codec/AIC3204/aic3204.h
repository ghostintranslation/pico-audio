#pragma once
#ifndef CODEC_H
#define CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/regs/rosc.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"

#include "pico/binary_info.h"
#include "pico/multicore.h"

#include "enums.h"
#include "codec/AIC3204/aic3204.h"
#include "codec/AIC3204/aic3204_i2s.pio.h"

#define I2C_SDA_CODEC 2
#define I2C_SCL_CODEC 3
#define I2C_PORT i2c1
#define I2S_ADC 4 // Input
#define I2S_DAC 5 // Outputs
#define I2S_LRCK 6
#define I2S_BCLK 7
#define I2S_MCLK 8
#define CODEC_RESET 9

// https://www.ti.com/lit/ds/symlink/tlv320aic3204.pdf
// https://www.ti.com/lit/ml/slaa557/slaa557.pdf - ref guide

// Audio Loop
int lringbuf[256];
int rringbuf[256];
int widx = 0;
int ridx = 0;

// Audio DMA
int tx_last_buffer = 0;
int rx_last_buffer = 0;

volatile int16_t tx_audio_buffer_0[AUDIO_BUFFER_SAMPLES * 2];
volatile int16_t tx_audio_buffer_1[AUDIO_BUFFER_SAMPLES * 2];
volatile int16_t rx_audio_buffer_0[AUDIO_BUFFER_SAMPLES * 2];
volatile int16_t rx_audio_buffer_1[AUDIO_BUFFER_SAMPLES * 2];

// PIO for I2S
PIO pio_aic3204 = pio0;
uint sm_aic3204 = 1;
uint offset_aic3204;

uint8_t tx_dma_channel = 0;
uint8_t rx_dma_channel = 0;

/////////////////////////////////////////////////////////////////////////
// Function Declarations
void init_codec();
void write_all_codec_regs();
void codec_enable_line_in(bool line, bool LR, bool mic);
void init_audio_pio();

// Assuming these functions are defined elsewhere
extern void process_audio(int16_t *buf);
extern void generate_audio(int16_t *buf);

/////////////////////////////////////////////////////////////////////////
// Function Implementations

void init_codec() {
    // call after stdio_init_all();

    // outputs
    // I2S pins

    // we don't use MCLK, but it is connected
    // it needs to be low
    gpio_init(I2S_MCLK);
    gpio_set_dir(I2S_MCLK, GPIO_OUT);
    gpio_put(I2S_MCLK, 0);
    gpio_set_pulls(I2S_MCLK, false, false);

    gpio_init(I2S_LRCK);
    gpio_set_dir(I2S_LRCK, GPIO_OUT);
    gpio_put(I2S_LRCK, 1);
    gpio_set_pulls(I2S_LRCK, false, false);

    gpio_init(I2S_BCLK);
    gpio_set_dir(I2S_BCLK, GPIO_OUT);
    gpio_put(I2S_BCLK, 1);
    gpio_set_pulls(I2S_BCLK, false, false);
    
    
    gpio_init(I2S_DAC);
    gpio_set_dir(I2S_DAC, GPIO_OUT);
    gpio_put(I2S_DAC, 1);
    gpio_set_pulls(I2S_DAC, false, false);

    // init pins
    // inputs
    gpio_init(I2S_ADC);
    gpio_set_dir(I2S_ADC, GPIO_IN); 
    gpio_set_pulls(I2S_ADC, false, false);

    // Codec reset
    gpio_init(CODEC_RESET);
    gpio_set_dir(CODEC_RESET, GPIO_OUT);
    gpio_set_pulls(CODEC_RESET, false, false);
    gpio_put(CODEC_RESET, 1); // set reset

    // Setup I2C
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA_CODEC, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_CODEC, GPIO_FUNC_I2C);

    bi_decl(bi_2pins_with_func(I2C_SDA_CODEC, I2C_SCL_CODEC, GPIO_FUNC_I2C));

    sleep_ms(2);

    write_all_codec_regs();
    sleep_ms(10);

    init_audio_pio();
}

/////////////////////////////////////////////////////////////////////////
// Helper functions for codec register writes

#define AIC3204_I2C_ADDR 0x18

void write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(I2C_PORT, AIC3204_I2C_ADDR, data, 2, false);
}

void goto_page(uint8_t page)
{
    write_reg(0, page);
}

void aic_write_page(uint8_t page, uint8_t reg, uint8_t val)
{
    goto_page(page);
    write_reg(reg, val);
}

void codec_enable_line_in(bool line, bool LR, bool mic) {
    goto_page(1);
    const static int enable_line_in = 0x80;         // (in1)
    const static int enable_LR_in = 0x20;           // (in2)
    const static int enable_mic_in = 0x04;          // (in3) 0x08 20k
    int enabled_inputs = 0;
    int mic_bias = 0;
    int mic_gain = 0;

    if(line) enabled_inputs += enable_line_in;      // Route 80=IN1L to LEFT_P, RIGHT_P with 20K input impedance
    if(LR) enabled_inputs += enable_LR_in;          // Route 20=IN2L to LEFT_P, RIGHT_P with 20K input impedance
    if(mic) {
        mic_bias += 0x40;           // MICBIAS powered up
        mic_bias += 0x20;           // 0x20 = 2.075V(CM = 0.75V) or MICBIAS = 2.5V(CM = 0.9V)
                                    // 0x10 = 1.425V(CM = 0.75V) or MICBIAS = 1.7V(CM = 0.9V)
                                    // 0x00 = 1.04V (CM = 0.75V) or MICBIAS = 1.25V(CM = 0.9V)

        write_reg(0x33, mic_bias);  // set it

        mic_gain = 50;       // MIC GAIN 101 1111: Volume Control = 47.5dB DEC 0 .. 95
        write_reg(0x3b, mic_gain);  // Left MICPGA Volume Control
        write_reg(0x3c, mic_gain);  // Right MICPGA Volume Control

        enabled_inputs += enable_mic_in;            // Route 8=IN3L to LEFT_P, RIGHT_P with 10K input impedance
    }

    write_reg(0x3d, 0x00);                          // Select ADC PTM_R4
    write_reg(0x34, enabled_inputs);                // Route 80=IN1L, 20=IN2L, 8=IN3L to LEFT_P with 20K input impedance
    write_reg(0x36, 0x80);                          // Route Common Mode to LEFT_N with impedance of 20K
    write_reg(0x37, enabled_inputs);                // Route 80=IN2R, 20=IN1R, 8=IN3L to RIGHT_P with input impedance

    goto_page(0);
}

void write_all_codec_regs(void) {
    aic_write_page(0x00, 0x01, 0x01); // software reset instruction
    sleep_ms(2);                      // not allowed to write regs for 1ms
    // first principles explanation for DAC :)
    // DOSR should be as high as possible, its the oversampler
    // 2.8MHz < DOSR * DAC_FS < 6.2MHz
    // at FS=32-48khz, DOSR=128 is fine. over 48khz, go 64 I think.
    // CODEC_CLKIN = NDAC * MDAC * DOSR * DAC_FS
    // but we must choose MDAC * DOSR / 32 ≥ RC
    // we want low sample rate, so filter B is fine
    // the stereo processing blocks for B are 7-11
    // if we choose 7, RC=6
    // if DOSR=128, MDAC*DOSR/32>=RC implies MDAC must be at least 3. lets choose 4.
    // for DOSR=64, double MDAC to at least 6.
    // how do we set NDAC? lets try 1.
    // so to summarise: DOSR=128, NDAC=1, MDAC=4, DAC_FS=48khz,

    // the PTM mode determines how hard the output swings and PSNR (+ power consumption)
    // PTM_P1 is quietest, up to PTM_P3 (high SNR too for PTM_P3!)
    // PTM_P4 is only for >16 bits

    // lets talk PLL! PLL_CLK = (PLL_CLKIN * R * J.D) / P
    // we are gonna drive it with bclk, which is 32*FS
    // PLL_CLK = (32*FS * R * J.D) / P
    // PLL_CLK is then divide by NDAC, MDAC and then DOSR which must give us FS.
    // ie 32*FS*R*J.D/P = FS*NDAC*MDAC*DOSR
    // FS and MDAC cancel, 32*J=DOSR
    // SO! J=DOSR/32=128/32=4

    // IN SUMMARY: input is BCLK=32*FS, DOSR=128, NDAC=1, MDAC=3 or more, P=1, D=0, R=MDAC=4, J=4
    const static int CRUNCH = 1; // overclock the PLL -> the codec -> maybe a bit more aliasing
    const static int MDAC = 4, NDAC = 1, DOSR = 128, P = 1, R = MDAC, J = CRUNCH * NDAC * DOSR / 32;

    // lets talk powersupply
    // we are gonna use the internal LDO to drive AVDD
    // we can run the headphone amp from LDOIN if we want (more digital noise tho?) for more swing
    //     'To use the higher supply voltage for higher output signal swing, the output
    // common-mode can be adjusted to either 1.25V, 1.5V or 1.65V by configuring Page 1, Register
    // 10, Bits D5-D4. When the common-mode voltage is configured at 1.65V and LDOIN supply is 3.3V,
    // the headphones can each deliver up to 40mW power into a 16Ω load.'

    // ldo control reg is page 1,reg 2
    // the ldo is typically gonna be 1.72v
    // it says elsewhere:
    // for analog supply voltages below 1.8V, a common mode voltage of 0.75V must be used.

    // reg 10 sets input common mode - stijn sets 0
    // this one is crucial!
    // D0 = 1 for LDOIN 3.3v. DONT SET THIS TO 0, OVERHEATS!
    // D1 = headphone is powered by 0=AVDD (smaller swing, lower noise), 1=LDOIN (3.3v, big swing)
    // D2 = must be 0
    // D3 = 0='same as chip', 1='lol and lor get big swing' (this is for the euro output)
    // D4-D5 = for headphone output: 00='same as chip', 11='3.3v big swing'
    // D6 = must be 0 for a 0.9v / 1.65v common mode (depending on D1)
    // D7 = must be 0

    // so I think the only choices are:
    // 0000?001 for 'low noise, lower swing'
    // or
    // 0011?011 for 'higher noise, bigger swing'
    // the ? bit controls the euro range, I think it should be '1' but can be set independently.
    // seems important to set bottom bit (D0) to 1 otherwise PAIN!
    const static int euro_big_swing = 1; // 1 or 0
    const static int hp_big_swing = 1;   // 1 or 0
    const static int common_mode_config = 1 + (hp_big_swing << 1) + (euro_big_swing << 3) + (hp_big_swing << 4) + (hp_big_swing << 5);
    // const static int common_mode_config = 0b01110001 + 8;

    // in register page 0, according to the datasheet we must do:
    // Program PLL clock dividers P,J,D,R (if PLL is necessary)
    // Power up PLL (if PLL is necessary)
    // Program and power up NDAC
    // Program and power up MDAC
    // Program OSR value
    // Program I2S word length if required (for example, 20bit)
    // Program the processing block to be used
    // lets do this!
    write_reg(4, 7);                  //  clock config -> blck->pll, pll->clkin
    write_reg(5, 128 + (P << 4) + R); // pll values for P and R
    write_reg(6, J);                  // 6 pll J
    write_reg(11, 128 + NDAC);        //  power up NDAC
    write_reg(12, 128 + MDAC);        //  power up MDAC
    // adc - same
    write_reg(18, NDAC); // XX WAS 5?  // power up NDAC
    write_reg(19, MDAC); // XX WAS 2? power up MDAC
    // back to dac config
    write_reg(13, DOSR >> 8);
    write_reg(14, DOSR & 255); // XX WAS 64
    write_reg(20, DOSR & 255); // also set AOSR for adc

    write_reg(27, 0);                   // i2s mode, 16 bits
    const static int PRB_BLOCK_DAC = 8; // 1=filter A, more aggressive lowpass; 7=filter B
    const static int PRB_BLOCK_ADC = 1; // 1=PRB_R1
    /// AOSR restricts the prb mode
    // 0000 0000: ADC AOSR = 256
    // 0010 0000: ADC AOSR = 32 (Use with PRB_R13 to PRB_R18, ADC Filter Type C)
    // 0100 0000: AOSR = 64 (Use with PRB_R1 to PRB_R12, ADC Filter Type A or B)
    // 1000 0000: AOSR = 128(Use with PRB_R1 to PRB_R6, ADC Filter Type A)

    write_reg(0x3c, PRB_BLOCK_DAC); // dac mode
    write_reg(0x3d, PRB_BLOCK_ADC); // adc mode

    // then we must do on page 1:
    // Disable coarse AVDD generation
    // Enable Master Analog Power Control
    // Program Common Mode voltage
    // Program PowerTune (PTM) mode - PRM_P1-3 only for 16 bit
    // Program Reference fast charging
    // Program Headphone specific depop settings (in case of headphone driver used)
    // Program routing of DAC output to the output amplifier (headphone or line out)
    // Unmute and set gain of output driver
    // Power up output driver
    // then wait a while (depop)
    goto_page(1);
    write_reg(1, 8); /* THIS WAS 0 WTF */ // -> disable crude vdd
    write_reg(2, 1);                      // -> enable power control , set LDO voltage, enable LDO
    write_reg(0x7b, 1);                   // -> ref charge time 40ms
    write_reg(0x14, 0x25);                //  -> soft step pop
    write_reg(10, common_mode_config);    //  -> see discussion about common mode
    // adc config
    const static int enable_LR_in = 0x20;           // (in2)
    const static int enable_line_in = 0x80;         // (in1)
    const static int enable_mic_in = 0x08;          // (in3)
    const static int enabled_inputs = enable_line_in+enable_LR_in; //enable_line_in; // enable_LR_in;
    // there is now a helper to update line in later.

    write_reg(0x3d, 0x00);           // Select ADC PTM_R4
    write_reg(0x34, enabled_inputs); // Route 80=IN1L, 20=IN2L to LEFT_P with 20K input impedance
    write_reg(0x36, 0x80);           // Route Common Mode to LEFT_N with impedance of 20K
    write_reg(0x37, enabled_inputs); // Route 80=IN2R, 20=IN1R to RIGHT_P with input impedance
                                    // of 20K; if '3'ish, 40k.
    write_reg(0x39, 0x80);           // Route Common Mode to RIGHT_N with impedance of 20K

    write_reg(0x3A, 0x80 + 0x40 + 0x20 + 0x10); // weakly connect in1l and in1r to cm

    // 12 is gain 1.837 on v5 hardware.
    // 10 is gain 1.626
    // 3 is gain 1.1
    // 0 is gain 0.93
    const static int adc_gain = 2; 
    write_reg(0x3b, adc_gain);      // set the adc pga gain in db. 4db seems to map 5v in to 4v out. see UNITY_GAIN_FACTOR_Q8
    write_reg(0x3c, adc_gain);

    // dac routing
    write_reg(0xc, 8); // 0x0c 08 left dac -> hpl; 0 is off
    write_reg(0xd, 8); // 0x0d 08 right dac -> hpr
    write_reg(0xe, 8); // 0x0c 08 left dac -> lol
    write_reg(0xf, 8); // 0x0d 08 right dac -> lor

    write_reg(3, 0);             // 3 0 -> ptm_p3/4, class A/B amp (not D)
    write_reg(4, 0);             // 4 0 -> ptm_p3/4, class A/B amp (not D)
    const static int hpgain = 0; // 0b111010;          // 6 gives us 3v p2p on the output (unloaded, measured)
    const static int logain = 0;
    const static int hpmute = 0;              // 64=muted, 0=not muted
    write_reg(0x10, (hpgain & 0x3f) + hpmute); // headphone gain
    write_reg(0x11, (hpgain & 0x3f) + hpmute);
    write_reg(0x12, logain & 0x3f); // line out gain
    write_reg(0x13, logain & 0x3f);
    write_reg(9, 0x30 + 4 + 8); // 9 0x30 power up hp drivers; 4+8 lol+lor powered up

    // now wait 2.5s to power up!
    // sleep_ms(2500);
    goto_page(0);          // 0 0 back to page 0
    write_reg(0x3f, 0xd6); // 0x3f d6 - power up dac channels

    // Power up Left and Right ADC Channels
    write_reg(0x51, 0xc0);
    // Unmute Left and Right ADC Digital Volume Control.
    write_reg(0x52, 0x00);

    goto_page(0);  // 0 0 back to page 0
    const static int vol = 0;
    write_reg(0x3f, 0xd6);  // 0x3f d6 - power up dac channels
    write_reg(0x41, vol);   // 0x41 vol -> left vol, signed, 0=0db
    write_reg(0x42, vol);   // 0x42 vol -> right vol, signed, 0=0db
    write_reg(0x40, 0);     // 0x40 00 -> unmute dac, set volume control
}

///////////////////////////////////////////////////////////////////////////
// DMA and PIO functions

void __isr __time_critical_func(audio_i2s_dma_irq_handler)()
{
    const int tx_dma_channel = 0;
    const int rx_dma_channel = 1;

    if (dma_hw->ints0 & (1 << tx_dma_channel))
    {
        dma_hw->ints0 = (1 << tx_dma_channel);

        if (tx_last_buffer == 0)
        {
            dma_channel_transfer_from_buffer_now(tx_dma_channel, tx_audio_buffer_1, AUDIO_BUFFER_SAMPLES * 1);
            tx_last_buffer = 1;
            generate_audio((int16_t *)tx_audio_buffer_0);
        }
        else
        {
            dma_channel_transfer_from_buffer_now(tx_dma_channel, tx_audio_buffer_0, AUDIO_BUFFER_SAMPLES * 1);
            tx_last_buffer = 0;
            generate_audio((int16_t *)tx_audio_buffer_1);
        }
    }

    if (dma_hw->ints0 & (1 << rx_dma_channel))
    {
        dma_hw->ints0 = (1 << rx_dma_channel);

        if (rx_last_buffer == 0)
        {
            dma_channel_transfer_to_buffer_now(rx_dma_channel, rx_audio_buffer_1, AUDIO_BUFFER_SAMPLES * 1);
            rx_last_buffer = 1;
            process_audio((int16_t *)rx_audio_buffer_0);
        }
        else
        {
            dma_channel_transfer_to_buffer_now(rx_dma_channel, rx_audio_buffer_0, AUDIO_BUFFER_SAMPLES * 1);
            rx_last_buffer = 0;
            process_audio((int16_t *)rx_audio_buffer_1);
        }
    }
}

void init_audio_pio(void) {
    pio_sm_claim(pio_aic3204, sm_aic3204);

    volatile uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    volatile uint32_t divider = system_clock_frequency * 2 / SAMPLE_FREQ;

    offset_aic3204 = pio_add_program(pio_aic3204, &audio_i2s_program);

    pio_gpio_init(pio_aic3204, I2S_ADC);
    pio_gpio_init(pio_aic3204, I2S_DAC);
    pio_gpio_init(pio_aic3204, I2S_BCLK);
    pio_gpio_init(pio_aic3204, I2S_LRCK);
    
    audio_i2s_program_init(pio_aic3204, sm_aic3204, offset_aic3204, I2S_ADC, I2S_DAC, I2S_LRCK);

    pio_sm_set_clkdiv_int_frac(pio_aic3204, sm_aic3204, divider >> 8, divider & 255);

    // queue some to avoid immediate underflow
    pio_aic3204->txf[0] = 0;
    pio_aic3204->txf[0] = 0;

    __mem_fence_release();

    tx_dma_channel = dma_claim_unused_channel(true);
    dma_channel_config tx_dma_config = dma_channel_get_default_config(tx_dma_channel);
    channel_config_set_write_increment(&tx_dma_config, false);
    channel_config_set_read_increment(&tx_dma_config, true);
    channel_config_set_transfer_data_size(&tx_dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&tx_dma_config, DREQ_PIO0_TX0 + sm_aic3204);
    dma_channel_configure(tx_dma_channel, 
                    &tx_dma_config,
                    &pio_aic3204->txf[sm_aic3204], // dest
                    NULL,                      // src
                    0,                         // count
                    false                      // trigger
    );
    dma_channel_set_irq0_enabled(tx_dma_channel, true);

    rx_dma_channel = dma_claim_unused_channel(true);
    dma_channel_config rx_dma_config = dma_channel_get_default_config(rx_dma_channel);
    channel_config_set_write_increment(&rx_dma_config, true);
    channel_config_set_read_increment(&rx_dma_config, false);
    channel_config_set_transfer_data_size(&rx_dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&rx_dma_config, DREQ_PIO0_TX0 + sm_aic3204);
    dma_channel_configure(rx_dma_channel, 
                    &rx_dma_config,
                    NULL,                      // dest
                    &pio_aic3204->rxf[sm_aic3204], // src
                    0,                         // count
                    false                      // trigger
    );
    dma_channel_set_irq0_enabled(rx_dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, audio_i2s_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    irq_set_priority(DMA_IRQ_0, 0);

    dma_channel_transfer_from_buffer_now(tx_dma_channel, tx_audio_buffer_0, BLOCK_SIZE * 1);
    dma_channel_transfer_to_buffer_now(rx_dma_channel, rx_audio_buffer_0, BLOCK_SIZE * 1);

    pio_sm_set_enabled(pio_aic3204, sm_aic3204, true);
}


#ifdef __cplusplus
}
#endif

#endif
