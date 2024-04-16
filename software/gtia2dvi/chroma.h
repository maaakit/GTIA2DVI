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

#define CHROMA_SAMPLES 250

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
    {
        chroma_table[sample][0] = compute_gtia_palette_color_adr(sample, 0);
        chroma_table[sample][1] = compute_gtia_palette_color_adr(sample, 1);
    }
}

static void inline calib_init()
{
    for (int i = 0; i <= MAX_SAMPLE; i++)
    {
        chroma_table[i][0] = -1;
        chroma_table[i][1] = -1;
    }
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
    counts[pos][row % 2]++;
}

static void inline calib_redraw(int row, int buf_seq)
{
    if (row == 60)
    {
        for (int c = 0; c < 15; c++)
        {
            for (int i = 0; i < SAMPLE_X_SIZE; i++)
                plotf(SAMPLE_X_FIRST + ((c * 25) / 2) + i, row - FIRST_GTIA_ROW_TO_SHOW, gtia_palette[(c + 1) * 16 + 8]);
        }
    }
    else if (row >= 59 && row <= 62)
    {
        for (uint x = 36; x < CHROMA_SAMPLES - 24; x++)
        {
            uint current_sample = decode_color(x, buf_seq);
            chroma_table[current_sample][row % 2] = 0;
        }
    }
    else
    {
        if (processing == false)
            for (uint x = 36; x < CHROMA_SAMPLES - 24; x++)
            {
                uint matched = decode_color(x, buf_seq);

                // uint color_bits = POPCNT(color_buf[buf_seq][x]);
                // if (color_bits < 13)
                // {
                //     chroma_table[matched][row % 2] = 0;
                // }

                plotf(x, row - FIRST_GTIA_ROW_TO_SHOW, matched == current_sample ? (row % 2 ? MAGENTA : YELLOW) : matched);

                if (row > SAMPLE_Y_FIRST && row < SAMPLE_Y_LAST)
                    if (matched == current_sample)
                        calib_count(row, x);
            }
        //     if (row > SAMPLE_Y_FIRST && row < SAMPLE_Y_LAST)
        //         plotf(30, row - FIRST_GTIA_ROW_TO_SHOW, RED);
    }
}

static uint16_t v_a = 0, v_b = 0;

uint16_t next_sample()
{
    v_a++;
    if ((v_a + v_b) > 32)
    {
        v_a = 0;
        v_b++;
    }
    return (v_b * 32) + v_a;
}

static void inline next_sample2()
{
    calib_clear();

    current_sample = next_sample();

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
static bool inline edge_only_pixels(int z)
{
    return false;
    // for (int i = 1; i < COUNTS - 1; i++)
    // {
    //     if (counts[i][z] == 0)
    //     {
    //         continue;
    //     }
    //     else
    //     {
    //         if (counts[i - 1][z] == 0 && counts[i + 1][z] == 0)
    //         {
    //             continue;
    //         }
    //         return false;
    //     }
    // }
    // return true;
}

static void inline calib_handle(int row)
{
    if (row == 5)
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
        if (row == 5 || row == 6)
        {
            uint z = row - 5;
            if (!edge_only_pixels(z))
            {

                uint max_sum = 0;
                uint max_index = -1;
                for (uint col = 0; col < 15; col++)
                {

                    uint pixel_sum = 0;
                    for (int i = 0; i < SAMPLE_X_SIZE; i++)
                    {
                        pixel_sum += counts[((col * 25) / 2) + SAMPLE_X_FIRST + i][z];
                    }
                    if (pixel_sum > 0)
                    {
                        if (max_sum < pixel_sum)
                        {
                            max_sum = pixel_sum;
                            max_index = col;
                        }
                        sprintf(buf, "%d-%d: %d", col + 1, z, pixel_sum);
                        UART_LOG_PUTLN(buf);
                    }
                }
                if (max_sum > MIN_CALIB_COUNT)
                {
                    chroma_table[current_sample][z] = max_index + 1;
                    uint x = current_sample >> 5;
                    uint y = current_sample & 0x1f;

                    plotf(280 + x, 40 + y + z * 40, gtia_palette[(max_index + 1) * 16 + 6]);
                }
            }
            plotf(266 + (current_sample % 4), 266 - (current_sample / 4), WHITE);
        }
        if (row == 7)
        {
            next_sample2();
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

static inline __inline uint8_t match_color(uint32_t color, uint32_t pal, int row)
{
    uint16_t sample = decode(color, pal);
    return chroma_table[sample][row % 2];
}

static uint16_t inline decode_color(int x, int buf_seq)
{
    uint32_t color = color_buf[buf_seq][x];
    uint32_t pal = pal_buf[buf_seq][x];
    return decode(color, pal);
}
#endif