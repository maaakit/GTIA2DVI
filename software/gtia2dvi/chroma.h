#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "hardware/interp.h"
#include "gtia_palette.h"
#include "chroma_trans_table.h"
#include "util/flash_storage.h"
#include "util/post_boot.h"

#ifndef CHROMA32_H
#define CHROMA32_H

#define CHROMA_LINE_LENGTH 206
#define CHROMA_LINE_LENGTH_16_BYTE_ALIGNED_SIZE (CHROMA_LINE_LENGTH + 3) & ~3

int8_t chroma_phase_adjust = -1;
uint8_t buf_seq = 0;
uint16_t frame_count = 0;

// chromabuf and each its sub array should be aligned to 16 bytes !
uint32_t chroma_buf[2][CHROMA_LINE_LENGTH_16_BYTE_ALIGNED_SIZE] __attribute__((aligned(16)));

static inline int16_t __not_in_flash_func(decode_intr)(uint32_t sample)
{

    interp0->accum[0] = 0;
    interp0->accum[1] = sample << 1;
    uint16_t *code_adr = (uint16_t *)interp0->pop[2];

    int16_t data = *code_adr;

    uint16_t result = data;
    interp0->add_raw[0] = (uint16_t)data;

    //                   ADD_RAW    MASK_MSB     MASK_LSB  SHIFT
    interp0->ctrl[1] = (1 << 18) + (11 << 10) + (1 << 5) + 11;
    code_adr = (uint16_t *)interp0->pop[2];
    data = *code_adr;

    result += data;
    interp0->add_raw[0] = (uint16_t)data;

    interp0->ctrl[1] = (1 << 18) + (11 << 10) + (1 << 5) + 0;
    interp0->accum[1] = sample >> 21;

    code_adr = (uint16_t *)interp0->pop[2];
    data = *code_adr;

    result += data;
    return result & 0x7ff;
}

static inline void __not_in_flash_func(chroma_hwd_init)()
{
    //                  ADD_RAW    MASK_MSB     MASK_LSB  SHIFT
    interp0->ctrl[0] = (1 << 18) + (15 << 10) + (12 << 5) + 0;
    interp0->ctrl[1] = (1 << 18) + (11 << 10) + (1 << 5) + 0;
    interp0->base[2] = (io_rw_32)trans_data;
}

#endif