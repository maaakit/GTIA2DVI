#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gfx.h"
#include "hardware/interp.h"
#include "flash_storage.h"
#include "gtia_palette.h"
#include "chroma_trans_table.h"
#include "post_boot.h"

#ifndef CHROMA_H
#define CHROMA_H

#define CHROMA_LINE_LENGTH 251

uint8_t buf_seq = 0;
uint32_t chroma_buf[2][CHROMA_LINE_LENGTH + 2];

enum calib_step
{
    WAIT = 0,
    STEP1 = 1,
    STEP2 = 2,
    STEP3 = 3,
    STEP4 = 4,
    TEST = 5,
    END = 99,
    ERROR = -1
};

enum calib_step c_step = WAIT;

uint16_t x1 = 226, x2 = 236;
uint16_t frame = 0;

static inline uint16_t __not_in_flash_func(decode_intr)(uint32_t sample)
{
    //                 ADD_RAW      MASK      MASK.   SHIFT
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 0;
    interp0->accum[0] = 0;
    interp0->accum[1] = sample << 1;      // 2
    uint16_t *code_adr = interp0->pop[2]; // 1
    uint16_t data = *code_adr;            // 1

#ifdef ERR_FAST_FAIL
    if (((int16_t)data) < 0)
    {
        return -1;
    }
#endif

    uint16_t result = data; // 2
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 7;
    code_adr = interp0->pop[2]; // 1
    data = *code_adr;

#ifdef ERR_FAST_FAIL
    if (((int16_t)data) < 0)
    {
        return -1;
    }
#endif

    result += data;
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 14;
    code_adr = interp0->pop[2];
    data = *code_adr;

#ifdef ERR_FAST_FAIL
    if (((int16_t)data) < 0)
    {
        return -1;
    }
#endif

    result += data;
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 21;
    code_adr = interp0->pop[2];
    data = *code_adr;

#ifdef ERR_FAST_FAIL
    if (((int16_t)data) < 0)
    {
        return -1;
    }
#endif

    result += data;
    return result & 0x7ff;
}

static inline void __not_in_flash_func(store_color)(int32_t sample, uint32_t row, uint32_t color)
{
    uint16_t dec = decode_intr(sample);
    if (dec > 0)
    {
        calibration_data[dec][row % 2] = color;
    }
}

static inline int8_t __not_in_flash_func(match_color)(int32_t sample, uint32_t row)
{
    uint16_t dec = decode_intr(sample);
    if (dec < 0)
        return -1;
    if (dec > 0)
    {
        return calibration_data[dec][row % 2];
    }
    return 0;
}

static inline void __not_in_flash_func(chroma_init)(bool preset_loaded)
{
    if (preset_loaded == false)
    {
        for (int i = 0; i < 2048; i++)
        {
            calibration_data[i][0] = 0xff;
            calibration_data[i][1] = 0xff;
        }
    }
    else
    {
        c_step = END;
    }
    store_color(0x0, 0, 0);
    store_color(0x0, 1, 0);
    interp0->ctrl[0] = (1 << 18) + (12 << 10) + (8 << 5) + 3;
    interp0->base[2] = trans_data;
}

static inline bool chroma_calibration_finished()
{
    return c_step == END;
}

void static inline _draw_rullers(uint16_t row)
{
    if (row == FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW + 2)
    {

        for (int x = 0; x < 200; x++)
            plot(x + 39, 150, x % 2 == 0 ? WHITE : RED);
    }
    if (row == FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW + 3)
    {

        for (int x = 0; x < 200; x++)
            plot(x + 39, 190, x % 2 == 0 ? WHITE : RED);
    }
}

static inline void _count_color(int32_t sample, uint32_t row, uint32_t color)
{
    uint32_t *cnt = framebuf;
    uint16_t dec = decode_intr(sample);
    if (dec < 0)
        return;
    cnt[((row % 2) << 12) + (dec << 4) + color]++;
}

static inline void _process_stats()
{
    uint32_t *cnt = framebuf;

    for (int row = 0; row < 2; row++)
    {
        for (int smpl = 0; smpl < 2048; smpl++)
        {
            int max=0;
            int max_col=0;
            for (int col = 0; col < 16; col++)
            {
                int t = cnt[((row % 2) << 12) + (smpl << 4) + col];
                if( t>max){
                    max=t;
                    max_col = col;
                }
            }
         calibration_data[smpl][row % 2] = max_col;

        }
    }
}

static inline void __not_in_flash_func(chroma_calibrate)(uint16_t row)
{
    if (c_step == WAIT && row == 280)
    {
        c_step = STEP1;
    }

    if (c_step == STEP1) // scan most right single color blocks
    {
        if (row > 68)
        {
            int col = (row - 69) / 12;
            if (col < 16)
            {
                if ((row - 69) % 12 <= 10)
                {
                    for (int i = x1; i < x2; i++)
                    {
                        uint32_t smpl = chroma_buf[buf_seq][i];
                        if (smpl != 0)
                        {
                            // sample and store block area
                            _count_color(smpl, row, col);
                        }
                    }

                    // sample right block edge (outher -> color 0)
                    _count_color(chroma_buf[buf_seq][239], row, 0);
                }
            }
        }

        if (frame == 200)
        {
            c_step = STEP2;
        }
    }

    if (c_step == STEP2)
    {
        if (row >= 169 && row < 259)
        {
            for (uint i = 1; i < 16; i++)
            {
                uint32_t smpl = chroma_buf[buf_seq][39 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, i);

                smpl = chroma_buf[buf_seq][39 + 1 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, i);

                smpl = chroma_buf[buf_seq][39 + 83 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, i);

                smpl = chroma_buf[buf_seq][39 + 83 + 1 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, i);

                uint c = ((row - 169) / 6) + 1;

                smpl = chroma_buf[buf_seq][39 + 3 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, c);
                smpl = chroma_buf[buf_seq][39 + 4 + (i - 1) * 5];
                if (smpl != 0)
         
                _count_color(smpl, row, c);
                smpl = chroma_buf[buf_seq][39 + 83 + 3 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, c);
                smpl = chroma_buf[buf_seq][39 + 83 + 4 + (i - 1) * 5];
                if (smpl != 0)
                _count_color(smpl, row, c);
            }
        }

        if (frame == 400 && row==300)
        {
            _process_stats();
            c_step = TEST;
        }
    }

    if (c_step == TEST)
    {
        for (int q = 0; q < CHROMA_LINE_LENGTH; q++)
        {
            int8_t matched = match_color(chroma_buf[buf_seq][q], row);
            uint16_t col565 = matched != -1 ? gtia_palette[matched * 16 + 4] : YELLOW;
            framebuf[(row - FIRST_GTIA_ROW_TO_SHOW) * FRAME_WIDTH + q] = col565;
        }

        _draw_rullers(row);
        if (frame == 600)
        {
            c_step = END;
            set_post_boot_action(WRITE_PRESET);
            watchdog_enable(1, 1);
        }
    }
}

#endif