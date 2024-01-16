#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gfx.h"
#include "hardware/interp.h"
#include "flash_storage.h"
#include "gtia_palette.h"
#include "chroma2_trans_table.h"

uint8_t fab2col[2048][2] __attribute__((section(".uninitialized_data")));

#define CHROMA_LINE_LENGTH 251

uint8_t buf_seq = 0;
uint32_t chroma_buf[2][CHROMA_LINE_LENGTH+2];

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

    // if (((int16_t)data) < 0)
    // {
    //     return -1;
    // }

    // branch here                            // 1
    uint16_t result = data; // 2
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 7;
    code_adr = interp0->pop[2]; // 1
    data = *code_adr;

    // if (((int16_t)data) < 0)
    // {
    // return -1;
    // }

    result += data;
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 14;
    code_adr = interp0->pop[2];
    data = *code_adr;

    // if (((int16_t)data) < 0)
    // {
    //     return -1;
    // }

    result += data;
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 21;
    code_adr = interp0->pop[2];
    data = *code_adr;

    // if (((int16_t)data) < 0)
    // {
    //     return -1;
    // }

    result += data;
    return result & 0x7ff;
}


static inline void __not_in_flash_func(store_color)(int32_t sample, uint32_t row, uint32_t color)
{
    uint16_t dec = decode_intr(sample);
    if (dec > 0)
    {
        fab2col[dec][row % 2] = color;
    }
}

static inline int8_t __not_in_flash_func(match_color)(int32_t sample, uint32_t row)
{
    uint16_t dec = decode_intr(sample);
    if (dec < 0)
        return -1;
    if (dec > 0)
    {
        return fab2col[dec][row % 2];
    }
    return 0;
}


static inline void __not_in_flash_func(chroma_init)(bool preset_loaded)
{
    if (preset_loaded == false)
    {
        for (int i = 0; i < 2048; i++)
        {
            fab2col[i][0] = 0xff;
            fab2col[i][1] = 0xff;
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

void static inline __not_in_flash_func(draw_rullers)(uint16_t row)
{
    if (row == 279)
    {

        for (int x = 0; x < 200; x++)
            plot(x + 39, 150, x % 2 == 0 ? WHITE : RED);
    }
    if (row == 280)
    {

        for (int x = 0; x < 200; x++)
            plot(x + 39, 190, x % 2 == 0 ? WHITE : RED);
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
        for (int q = 0; q < CHROMA_LINE_LENGTH; q++)
            framebuf[(row - 43) * FRAME_WIDTH + q] = chroma_buf[buf_seq][q];
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
                            store_color(smpl, row, col);
                            plot(i, 239, WHITE);
                        }
                    }

                    // sample right block edge (outher -> color 0)
                    store_color(chroma_buf[buf_seq][239], row, 0);
                    plot(252, row - 43, gtia_palette[col * 16 + 4]);
                }
            }
        }

        if (frame == 200)
        {
            c_step = STEP2;
            plot(395, 239, RED);
        }
    }

    if (c_step == STEP2)
    {
        if (row >= 169 && row < 259)
        {
            for (uint i = 1; i < 16; i++)
            {
                plot(39 + (i - 1) * 5, row - 43, GREEN);
                plot(39 + 83 + (i - 1) * 5, row - 43, RED);

                uint32_t smpl = chroma_buf[buf_seq][39 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, i);

                smpl = chroma_buf[buf_seq][39 + 1 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, i);

                smpl = chroma_buf[buf_seq][39 + 83 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, i);

                smpl = chroma_buf[buf_seq][39 + 83 + 1 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, i);

                uint c = ((row - 169) / 6) + 1;

                plot(39 + 3 + (i - 1) * 5, row - 43, gtia_palette[c * 16 + 8]);

                smpl = chroma_buf[buf_seq][39 + 3 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, c);
                smpl = chroma_buf[buf_seq][39 + 4 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, c);
                smpl = chroma_buf[buf_seq][39 + 83 + 3 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, c);
                smpl = chroma_buf[buf_seq][39 + 83 + 4 + (i - 1) * 5];
                if (smpl != 0)
                    store_color(smpl, row, c);
            }
        }

        if (frame == 400)
        {
            c_step = TEST;
            plot(393, 239, BLUE);
        }
    }

    if (c_step == TEST)
    {

        for (int q = 0; q < CHROMA_LINE_LENGTH; q++)
        {
            int8_t matched = match_color(chroma_buf[buf_seq][q], row);
            uint16_t col565 = matched != -1 ? gtia_palette[matched * 16 + 4] : YELLOW;
            framebuf[(row - 43) * FRAME_WIDTH + q] = col565;
        }

        draw_rullers(row);
        if (frame == 600)
        {
            c_step = END;
            //  plot(393, 239, BLUE);
            plot(391, 239, YELLOW);
            save_preset(fab2col);
  
        }
    }
}

static inline uint16_t __not_in_flash_func(decode_intr2)(uint32_t sample)
{
    uint16_t aggr = 0;
    uint32_t page_offset = 0;

    if (sample == 0)
    {
        return 0;
    }

    uint16_t val = trans_data[sample & 0x7f];
    if (((int16_t)val) >= 0)
    {
        aggr += (val & 0x7ff);
        sample >>= 7;
        page_offset += ((val >> 4) & 0x780);

        val = trans_data[page_offset + (sample & 0x7f)];
        if (((int16_t)val) >= 0)
        {
            aggr += (val & 0x7ff);
            sample >>= 7;
            page_offset += ((val >> 4) & 0x780);

            val = trans_data[page_offset + (sample & 0x7f)];
            if (((int16_t)val) >= 0)
            {
                aggr += (val & 0x7ff);
                sample >>= 7;
                page_offset += ((val >> 4) & 0x780);

                val = trans_data[page_offset + (sample & 0x7f)];
                if (((int16_t)val) >= 0)
                {
                    aggr += (val & 0x7ff);
                    return aggr & 0x7ff;
                }
            }
        }
    }
    return -1;
}

