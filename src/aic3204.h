#pragma once 
#ifndef CODEC_H
#define CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "enums.h"

void init_codec();
void write_all_codec_regs();
void codec_enable_line_in(bool line, bool LR, bool mic);
void init_audio_pio();

extern PIO pio_aic3204;
extern uint sm_aic3204;
extern uint offset_aic3204;

extern int lringbuf[256];
extern int rringbuf[256];
extern int widx;
extern int ridx;

extern void process_audio(int16_t *buf);
extern void generate_audio(int16_t *buf);


#ifdef __cplusplus
}
#endif

#endif