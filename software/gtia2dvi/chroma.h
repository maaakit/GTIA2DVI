#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "hardware/interp.h"
#include "gtia_palette.h"
#include "chroma_trans_table.h"
#include "util/flash_storage.h"
#include "util/post_boot.h"
#include "uart_dump.h"

#ifndef CHROMA_H
#define CHROMA_H

#define CHROMA_LINE_LENGTH 202
#define CALIB_X1 226
#define CALIB_X2 236

uint16_t frame = 0;
uint8_t buf_seq = 0;
uint32_t chroma_buf[2][CHROMA_LINE_LENGTH + 2];

#define CALIBRATION_FIRST_BAR_CHROMA_INDEX 31
#define SAMPLING_FRAMES 3

enum calib_step
{
    STEP1 = 1,
    STEP2,
    SAVE
};

enum calib_step c_step = STEP1;

static inline int16_t __not_in_flash_func(decode_intr)(uint32_t sample)
{
    //                 ADD_RAW      MASK      MASK.   SHIFT
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 0;
    interp0->accum[0] = 0;
    interp0->accum[1] = sample << 1;
    uint16_t *code_adr = interp0->pop[2];
    int16_t data = *code_adr;

#ifdef ERR_FAST_FAIL
    if (((int16_t)data) < 0)
    {
        return -1;
    }
#endif

    int16_t result = data;
    interp0->add_raw[0] = data;
    interp0->ctrl[1] = (1 << 18) + (7 << 10) + (1 << 5) + 7;
    code_adr = (uint16_t *)interp0->pop[2];
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
    code_adr = (uint16_t *)interp0->pop[2];
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
    code_adr = (uint16_t *)interp0->pop[2];
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
    int16_t dec = decode_intr(sample);
    if (dec < 0)
        return -1;
    if (dec > 0)
    {
        return calibration_data[dec][row % 2];
    }
    return 0;
}

static inline void __not_in_flash_func(chroma_calibration_init)()
{
    for (int i = 0; i < 2048; i++)
    {
        calibration_data[i][0] = 0xff;
        calibration_data[i][1] = 0xff;
    }
    store_color(0x0, 0, 0);
    store_color(0x0, 1, 0);
}

static inline void __not_in_flash_func(chroma_hwd_init)()
{
    interp0->ctrl[0] = (1 << 18) + (12 << 10) + (8 << 5) + 3;
    interp0->base[2] = (io_rw_32)trans_data;
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
    uint32_t *cnt = (uint32_t *)framebuf;
    uint16_t dec = decode_intr(sample);
    if (dec < 0)
        return;

    // PAL GTIA colors 1 and 15 are the same, so use only 1
    // palette colors 240-255 are used for menu screen
    if (color == 15)
        color = 1;

    cnt[((row % 2) << 12) + (dec << 4) + color]++;
}

static inline void _process_stats()
{
    uint32_t *cnt = (uint32_t *)framebuf;

    for (int row = 0; row < 2; row++)
    {
        for (int smpl = 0; smpl < 2048; smpl++)
        {
            int max = 0;
            int max_col = 0;
            for (int col = 0; col < 16; col++)
            {
                int t = cnt[((row % 2) << 12) + (smpl << 4) + col];
                if (t > max)
                {
                    max = t;
                    max_col = col;
                }
            }
            calibration_data[smpl][row % 2] = max_col;
        }
    }
}

static inline void __not_in_flash_func(calibration_step1)(uint16_t row)
{
    if (row > 68)
    {
        int col = (row - 69) / 12;
        if (col < 16)
        {
            if ((row - 69) % 12 <= 10)
            {
                for (int i = CALIB_X1; i < CALIB_X2; i++)
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
}
static inline void __not_in_flash_func(calibration_step2)(uint16_t row)
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
}

// uint16_t __not_in_flash("counts") counts[COUNTS][2];
uint16_t current_sample = 0;
uint32_t sample_frame = 0;
bool processing = false;

int8_t mapping[2][4][2048];

static inline void store_color_x(int row, int x, int16_t decode, int8_t color_index)
{
    if (decode < 0)
        return;

    if (color_index > 15 || color_index < 0)
        return;

    if (color_index == 15)
        color_index = 1;

    if (mapping[row % 2][x % 4][decode] == -1)
    {
        mapping[row % 2][x % 4][decode] = color_index;

        if (color_index < 1)
        {
            return;
        }
    }
}

#define MAP_DIAGRAM_BOX_SIZE 36
#define MAP_DIAGRAM_OFFSET_X 240
#define MAP_DIAGRAM_OFFSET_Y 16

static inline void update_mapping_diagram(int16_t decode)
{
    int f = decode & 0x1;
    int a = (decode >> 1) & 0x1f;
    int b = (decode >> 6) & 0x1f;
    int row, color_index, color;

    for (int mod = 0; mod < 4; mod++)
    {
        row = 0;
        color_index = mapping[row][mod][decode];
        color = (color_index > 0) ? color_index * 16 + 6 : 2;
        plotf(MAP_DIAGRAM_OFFSET_X + (mod)*MAP_DIAGRAM_BOX_SIZE + a, MAP_DIAGRAM_OFFSET_Y + (row)*MAP_DIAGRAM_BOX_SIZE + (f * MAP_DIAGRAM_BOX_SIZE * 2) + b, color);

        row = 1;
        color_index = mapping[row][mod][decode];
        color = (color_index > 0) ? color_index * 16 + 6 : 2;
        plotf(MAP_DIAGRAM_OFFSET_X + (mod)*MAP_DIAGRAM_BOX_SIZE + a, MAP_DIAGRAM_OFFSET_Y + (row)*MAP_DIAGRAM_BOX_SIZE + (f * MAP_DIAGRAM_BOX_SIZE * 2) + b, color);
    }
}

static inline void update_mapping_diagram_c(int16_t dec, uint i, uint row, int col)
{
    int f = dec & 0x1;
    int a = (dec >> 1) & 0x1f;
    int b = (dec >> 6) & 0x1f;

    plotf(MAP_DIAGRAM_OFFSET_X + (i % 4) * MAP_DIAGRAM_BOX_SIZE + a, MAP_DIAGRAM_OFFSET_Y + (row % 2) * MAP_DIAGRAM_BOX_SIZE + (f * MAP_DIAGRAM_BOX_SIZE * 2) + b, col);
}

static inline void update_mapping_diagram_err(int16_t dec, uint i, uint row)
{
    update_mapping_diagram_c(dec, i, row, WHITE);
}

static bool err_draw = false;
static bool fine_tuned = false;

static inline int8_t fine_tune(int16_t dec, uint16_t x, uint16_t row)
{
    // vertical boxes on the right side
    if (x >= 182 && x <= 189)
    {
        if (row > 80 && row < 260)
        {
            int col = 1 + ((row - 80) / 12);

            if (col == 15)
                col = 1;

            if (col > 0 && col <= 15)
            {
                store_color_x(row, x, dec, col);
                update_mapping_diagram_c(dec, x, row, col * 16 + 6);
                return col;
            }
        }
        return -1;
    }
    else
    {
        // vertical lines of solid colors on top part of screen
        if (row > 75 && row < 150)
        {
            uint8_t col = 1 + (x - CALIBRATION_FIRST_BAR_CHROMA_INDEX) / 10;
            if (col == 15)
                col = 1;

            if (x >= CALIBRATION_FIRST_BAR_CHROMA_INDEX && x < CALIBRATION_FIRST_BAR_CHROMA_INDEX + 150)
            {
                if (((x - CALIBRATION_FIRST_BAR_CHROMA_INDEX) % 10) < 8)
                {
                    if (col > 0 && col <= 15)
                    {
                        store_color_x(row, x, dec, col);
                        update_mapping_diagram_c(dec, x, row, col * 16 + 6);
                        return col;
                    }
                }
                return 0;
            }
        }
        else if (row >= 169 && row <= 262)
        {
            int px;

            if (x >= 96)
                px = x - 96;
            else
                px = x - 30;

            if (px % 4 < 2)
            {
                // strip
                uint8_t col = 1 + (px / 4);
                if (col == 15)
                    col = 1;
                if (col > 0 && col < 15)
                {
                    store_color_x(row, x, dec, col);
                    update_mapping_diagram_c(dec, x, row, col * 16 + 6);
                    return col;
                }
            }
        }
    }
    return -1;
}

static inline void __not_in_flash_func(chroma_calibrate_step1)(uint16_t row)
{
    err_draw = false;
    fine_tuned = false;

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

        if (row == 10)
        {
            if (current_sample < 0x7ff)
            {
                // update mapping diagram
                update_mapping_diagram(current_sample);

                current_sample++;
                processing = false;
                sample_frame = 0;
            }
            else
            {
                c_step = STEP2;
            }
        }
        // draw progress bar
        plotf(8 + (current_sample / 8), 270 + (current_sample % 8), GREEN);
    }

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (row == 64)
    {
        // draw expected colors row
        for (int x = CALIBRATION_FIRST_BAR_CHROMA_INDEX; x < CALIBRATION_FIRST_BAR_CHROMA_INDEX + 150; x++)
        {
            int col = 1 + (x - CALIBRATION_FIRST_BAR_CHROMA_INDEX) / 10;
            if (col == 15)
                col = 1;

            if (((x - CALIBRATION_FIRST_BAR_CHROMA_INDEX) % 10) < 8)
                plotf(x, y, col * 16 + 6);
        }
    }

    // map black color
    if (row > 50 && row < 60)
    {
        for (int x = 32; x < 64; x++)
        {
            uint sample = chroma_buf[buf_seq][x];
            int dec = decode_intr(sample);
            if (sample == 0)
                store_color_x(row, x, dec, BLACK);

            // mapping[row % 2][x % 4][dec] = 0;
            plotf(x, y, BLACK);
        }
    }

    if (row > 66 && row < 266)
    {
        for (uint32_t i = 28; i < (CHROMA_LINE_LENGTH - 8); i++)
        {
            uint sample = chroma_buf[buf_seq][i];
            int dec = decode_intr(sample);

            if (dec == current_sample && row > 75 && row < 150)
            {
                // mark matching pixels
                plotf(i, y, row % 2 ? RED : MAGENTA);

                if (sample != 0)
                {
                    if (dec == current_sample)
                    {
                        uint8_t col = 1 + (i - CALIBRATION_FIRST_BAR_CHROMA_INDEX) / 10;
                        if (col == 15)
                            col = 1;

                        if (i >= CALIBRATION_FIRST_BAR_CHROMA_INDEX && i < CALIBRATION_FIRST_BAR_CHROMA_INDEX + 150)
                        {
                            if (((i - CALIBRATION_FIRST_BAR_CHROMA_INDEX) % 10) < 8)
                            {
                                if (col > 0 && col <= 15)
                                    store_color_x(row, i, dec, col);
                            }
                            else
                                store_color_x(row, i, dec, BLACK);
                        }
                        else if (i == CALIBRATION_FIRST_BAR_CHROMA_INDEX - 1)
                        {
                            store_color_x(row, i, dec, BLACK);
                        }
                    }
                }
            }
            // draw already mapped colors
            else if (dec < current_sample)
            {
                int8_t col = mapping[row % 2][i % 4][dec];

                if (col == 0)
                    // mapped black
                    plotf(i, y, BLACK);

                else if (col > 15 || col < 0)
                // color not mapped
                {
                    // if (    fine_tuned == false)
                    // {
                    //     col = fine_tune(dec, i, row);

                    //     if (col > 0)
                    //         plotf(i, y, col * 16 + 6);
                    //     else
                    //         plotf(i, y, YELLOW);
                    //     fine_tuned = true;
                    // }
                    // else
                    {
                        plotf(i, y, YELLOW);
                        if (err_draw == false)
                        {
                            err_draw = true;
                            update_mapping_diagram_err(dec, i, row);
                        }
                    }
                }
                else
                    // mapped valid GTIA color
                    plotf(i, y, col * 16 + 6);
            }

            else
                plotf(i, y, sample == 0 || dec < 0 ? BLACK : GRAY1);
        }
    }
}

static inline void __not_in_flash_func(chroma_calibrate_step2)(uint16_t row)
{
    err_draw = false;
    fine_tuned = false;

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (row > 66 && row < 266)
    {
        for (uint32_t i = 28; i < (CHROMA_LINE_LENGTH - 8); i++)
        {
            uint sample = chroma_buf[buf_seq][i];
            int dec = decode_intr(sample);

            int8_t col = mapping[row % 2][i % 4][dec];

            if (col == 0)
                // mapped black
                plotf(i, y, BLACK);

            else if (col > 15 || col < 0)
            // color not mapped
            {
                if (fine_tuned == false)
                {
                    col = fine_tune(dec, i, row);

                    if (col > 0)
                    {
                        plotf(i, y, col * 16 + 6);
                      //  update_mapping_diagram_c(dec, i, row, col * 16 + 6);
                    }
                    else
                    {
                        plotf(i, y, YELLOW);
                        update_mapping_diagram_err(dec, i, row);
                    }
                    fine_tuned = true;
                }
                else
                {
                    plotf(i, y, YELLOW);
                    if (err_draw == false)
                    {
                        err_draw = true;
                        update_mapping_diagram_err(dec, i, row);
                    }
                }
            }
            else
                // mapped valid GTIA color
                plotf(i, y, col * 16 + 6);
        }
    }
}

static inline void __not_in_flash_func(chroma_calibrate)(uint16_t row)
{
    switch (c_step)
    {
    case STEP1:
        chroma_calibrate_step1(row);
        break;
    case STEP2:
        chroma_calibrate_step2(row);
        break;

    default:
        break;
    }
}

#endif