#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "hardware/interp.h"
#include "gtia_palette.h"
#include "util/flash_storage.h"
#include "util/post_boot.h"
#include "popcounts.h"

#ifndef CHROMA_H
#define CHROMA_H

#define CHROMA_SAMPLES 202

#define POPCNT popcount3

uint16_t __not_in_flash("counts") counts[COUNTS][2];
uint16_t current_sample = 0;
uint32_t sample_frame = 0;
bool processing = false;

uint16_t frame = 0;
uint8_t buf_seq = 0;
uint32_t color_buf[2][CHROMA_SAMPLES];
uint32_t pal_buf[2][CHROMA_SAMPLES];

static uint16_t __scratch_y("decode_color") decode_color(int x, int buf_seq);

static inline uint16_t compute_gtia_palette_color_adr(int sample, int row)
{
    int8_t col = chroma_table[sample][row];
    if (col < 0 || col > 15)
    {
        col = NOT_MATCHED_COLOR_INDEX;
    }
    return col;
}

static void inline gtia_color_trans_table_init()
{
    for (int sample = 0; sample <= MAX_SAMPLE; sample++)
        for (uint x = 0; x < 10; x++)
            chroma_table[sample][x] = compute_gtia_palette_color_adr(sample, x);
}

static void inline calib_init()
{
    for (int i = 0; i <= MAX_SAMPLE; i++)
        for (uint x = 0; x < 10; x++)
            chroma_table[i][x] = -1;
}

static void inline calib_clear()
{
    for (int i = 0; i < COUNTS; i++)
    {
        counts[i][0] = 0;
        counts[i][1] = 0;
    }
    sample_frame = 0;
}

static void inline calib_count(int row, int pos)
{
    counts[pos - SAMPLE_X_FIRST][row % 2]++;
}

static void inline calib_redraw(int row, int buf_seq)
{
    if (row == SAMPLE_Y_FIRST - 4)
    {
        for (int c = 0; c < 15; c++)
        {
            for (int i = 0; i < SAMPLE_X_SIZE; i++)
                plotf(SAMPLE_X_FIRST + (c * 10) + i, row - FIRST_GTIA_ROW_TO_SHOW, gtia_palette[(c + 1) * 16 + 8]);
        }
    }
    else if (row >= SAMPLE_Y_FIRST - 10 && row <= SAMPLE_Y_FIRST - 2)
    {
        for (uint x = SAMPLE_X_FIRST; x < CHROMA_SAMPLES - 20; x++)
        {
            uint current_sample = decode_color(x, buf_seq);
            int off = x % 5;
            int z = row % 2;
            chroma_table[current_sample][off * 2 + z] = 0;
        }
    }
    else
    {
        if (processing == false)
            for (uint x = SAMPLE_X_FIRST; x < CHROMA_SAMPLES - 20; x++)
            {
                uint matched = decode_color(x, buf_seq);

                plotf(x, row - FIRST_GTIA_ROW_TO_SHOW, matched == current_sample ? (row % 2 ? MAGENTA : YELLOW) : matched);

                if (row > SAMPLE_Y_FIRST && row < SAMPLE_Y_LAST)
                    if (matched == current_sample)
                        calib_count(row, x);
            }
    }
}

static uint16_t v_a = 0, v_b = 1;

uint16_t calc_next_sample()
{
    v_a++;
    if ((v_a + v_b) > 32)
    {
        v_a = 0;
        v_b++;
    }
    return (v_b * 32) + v_a;
}

static void inline switch_to_next_sample()
{
    calib_clear();

    current_sample = calc_next_sample();

    if (current_sample > MAX_SAMPLE)
    {
        UART_LOG_PUTLN("calibration finished");
        UART_LOG_PUTLN("saving preset");
        app_cfg.enableChroma = true;
        set_post_boot_action(WRITE_CONFIG);
        set_post_boot_action(WRITE_PRESET);
        watchdog_enable(500, 1);
        while (true)
        {
            UART_LOG_FLUSH();
        }
    }
}

static void inline calib_handle(int row)
{
    if (row == 2)
    {
        sample_frame++;
        if (sample_frame == SAMPLING_FRAMES)
        {
            processing = true;
            sprintf(buf, "SMPL: %04x", current_sample);
            UART_LOG_PUTLN(buf);
        }
    }
    if (processing == true)
    {
        if (row >= 2 && row <= 11)
        {
            uint z = (row - 2) / 5;
            uint off = (row - 2) % 5;

            uint max_sum = 0;
            uint max_index = -1;
            for (uint col = 0; col < 15; col++)
            {
                uint pixel_sum = 0;
                for (int i = 0; i < SAMPLE_X_SIZE; i = i + 5)
                {
                    pixel_sum += counts[(col * 10) +  /* SAMPLE_X_FIRST*/ + i  + off][z];
                }
                if (pixel_sum > 0)
                {
                    if (max_sum < pixel_sum)
                    {
                        max_sum = pixel_sum;
                        max_index = col;
                    }
                    sprintf(buf, "%x_%d%d %x", col + 1, z, off, pixel_sum);
                    UART_LOG_PUTLN(buf);
                }
            }
            if (max_sum > MIN_CALIB_COUNT)
            {
                chroma_table[current_sample][off * 2 + z] = max_index + 1;
                uint x = current_sample >> 5;
                uint y = current_sample & 0x1f;

                plotf(280 + x + z * 40, 40 + y + off * 40, gtia_palette[(max_index + 1) * 16 + 6]);
                // plotf(280 + x, 40 + y + z * 40, gtia_palette[(max_index + 1) * 16 + 6]);
                // plotf(280 + x, 40 + y + z * 40, gtia_palette[(max_index + 1) * 16 + 6]);
                
                // sprintf(buf, ">%d@%d%d", max_index + 1, z, off);
                // UART_LOG_PUTLN(buf);
            }

            plotf(266 + (current_sample % 4), 266 - (current_sample / 4), WHITE);
        }
        if (row == 12)
        {
            switch_to_next_sample();
            processing = false;
        }
    }
    UART_LOG_FLUSH();
}

//__attribute__((noinline))

static int8_t inline __inline popcount(uint32_t data)
{
    return POPCNT(data);
}

static const uint16_t inline decode(uint32_t color, uint32_t pal)
{
    uint32_t v1 = color & (~pal);
    uint32_t v2 = (~color) & pal;

    uint32_t vv1 = POPCNT(v1);
    uint32_t vv2 = POPCNT(v2);

    return vv1 << 5 | vv2;
}

static inline __inline uint8_t match_color(uint32_t color, uint32_t pal, int idx)
{
    uint16_t sample = decode(color, pal);
    return chroma_table[sample][idx];
}

static uint16_t inline decode_color(int x, int buf_seq)
{
    uint32_t color = color_buf[buf_seq][x];
    uint32_t pal = pal_buf[buf_seq][x];
    return decode(color, pal);
}
#endif