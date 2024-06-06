#ifndef CHROMA_CALIBRATION_H
#define CHROMA_CALIBRATION_H

#include "chroma.h"
#include "chroma_map_diagram.h"

#define CALIBRATION_FIRST_BAR_CHROMA_INDEX 31
#define SAMPLING_FRAMES 10

enum calib_step
{
    STEP1 = 1,
    STEP2,
    SAVE
};

enum calib_step c_step = STEP1;

uint16_t current_sample = 0;
uint32_t sample_frame = 0;
bool processing = false;

int16_t color_counts[2][4][15];

static inline void store_color_x(int row, int x, int16_t decode, int8_t color_index)
{
    if (decode < 0)
        return;

    if (color_index > 15 || color_index < 0)
        return;

    if (color_index == 15)
        color_index = 1;

    if (calibration_data[row % 2][x % 4][decode] == -1)
    {
        calibration_data[row % 2][x % 4][decode] = color_index;

        if (color_index < 1)
        {
            return;
        }
    }
}

static inline void color_counts_clear()
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 15; j++)
        {
            color_counts[0][i][j] = 0;
            color_counts[1][i][j] = 0;
        }
}

static inline void color_counts_add(int row, int x, int8_t color_index)
{
    color_counts[row % 2][x % 4][color_index]++;
}

static inline void color_counts_process(int row, uint16_t current_sample)
{
    int mod_row = row % 2;
    int mod_x = (row / 2) % 4;
    int max_i = 0, max_cnt = 0;

    for (int i = 0; i < 15; i++)
    {
        if (color_counts[mod_row][mod_x][i] > max_cnt)
        {
            max_cnt = color_counts[mod_row][mod_x][i];
            max_i = i;
        }
    }
    if (max_i > 0)
    {
        store_color_x(mod_row, mod_x, current_sample, max_i);
    }
}

static inline void __not_in_flash_func(chroma_calibration_init)()
{
    for (int x = 0; x < 2048; x++)
    {
        for (int y = 0; y < 4; y++)
        {
            calibration_data[0][y][x] = -1;
            calibration_data[1][y][x] = -1;
        }
    }
}

static inline bool validate(int16_t dec, uint16_t x, uint16_t row, int8_t color, int treshold)
{
    int f = dec & 0x1;
    int a = (dec >> 1) & 0x1f;
    int b = (dec >> 6) & 0x1f;

    for (int aa = MAX(0, a - 1); aa < MIN(32, a + 1); aa++)
        for (int bb = MAX(0, b - 1); bb < MIN(32, b + 1); bb++)
        {
            int16_t dec2 = f | aa << 1 | bb << 6;
            int8_t neightbour = calibration_data[row % 2][x % 4][dec2];

            if (neightbour <= 0)
                continue;
            else
            {
                uint diff = MIN(abs(neightbour - color), 14 - abs(neightbour - color));
                if (diff > treshold)
                    return false;
            }
        }
    return true;
}

static inline int8_t __not_in_flash_func(update_if_valid)(int16_t dec, uint16_t x, uint16_t row, int col, int treshold)
{
    if (col == 15)
        col = 1;

    if (col > 0 && col <= 15)
    {
        if (validate(dec, x, row, col, treshold))
        {
            store_color_x(row, x, dec, col);
            update_mapping_diagram_c(dec, x, row, col * 16 + 6);
            return col;
        }
        else
            return -1;
    }
    else
    {
        __breakpoint();
    }
}

static inline int8_t fine_tune(int16_t dec, uint16_t x, uint16_t row)
{
    // vertical boxes on the right side
    if (x >= 182 && x <= 190)
    {
        if (row > 80 && row < 260)
        {
            int col = 1 + ((row - 80) / 12);
            int valid = update_if_valid(dec, x, row, col, 15);
            return valid;
        }
        return -1;
    }
    else
    {
        // vertical lines of solid colors on top part of screen
        if (row > 75 && row < 150)
        {
            uint8_t col = 1 + (x - CALIBRATION_FIRST_BAR_CHROMA_INDEX) / 10;

            if (x >= CALIBRATION_FIRST_BAR_CHROMA_INDEX && x < CALIBRATION_FIRST_BAR_CHROMA_INDEX + 150)
            {
                if (((x - CALIBRATION_FIRST_BAR_CHROMA_INDEX) % 10) < 8)
                {

                    return update_if_valid(dec, x, row, col, 15);
                }
                return 0;
            }
        }
        else if (row >= 169 && row <= 258)
        {
            int px;
            if (x >= 96)
                px = x - 96;
            else
                px = x - 30;

            if (px < 60)
            {
                if (px % 4 < 2)
                {
                    // strip
                    uint8_t col = 1 + (px / 4);
                    return update_if_valid(dec, x, row, col, 1);
                }
                else
                {
                    uint8_t col = 1 + ((row - 169) / 6);
                    return update_if_valid(dec, x, row, col, 1);
                }
            }
        }
    }
    return -1;
}
static bool err_draw = false;
static bool fine_tuned = false;

static inline void _log_error_value(int16_t dec, uint i, uint16_t row)
{
    if (err_draw == false)
    {
        err_draw = true;
        update_mapping_diagram_err(dec, i, row);
    }
}

static inline void _save()
{
    uart_log_putln("requesting calibration data save");
    uart_log_flush_blocking();
    app_cfg.enableChroma = true;
    set_post_boot_action(WRITE_CONFIG);
    set_post_boot_action(WRITE_PRESET);
    watchdog_enable(1, 1);
}

static inline void __not_in_flash_func(chroma_calibrate_step1)(uint16_t row)
{
    err_draw = false;

    if (row == 1)
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
        if (row >= 2 && row <= 9)
        {
            color_counts_process(row, current_sample);
        }
        if (row == 10)
        {
            if (current_sample < 0x7ff)
            {
                // update mapping diagram
                update_mapping_diagram(current_sample);
                color_counts_clear();
                current_sample++;
                processing = false;
                sample_frame = 0;
            }
            else
            {
                c_step = STEP2;
                sample_frame = 0;

                uart_log_putln("entering calibration step 2");
                uart_log_flush_blocking();
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
        for (uint32_t i = 30; i < (CHROMA_LINE_LENGTH - 10); i++)
        {
            uint sample = chroma_buf[buf_seq][i];
            int dec = decode_intr(sample);

            if (dec == current_sample && row > 70 && row < 155)
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
                                    // store_color_x(row, i, dec, col);
                                    color_counts_add(row, i, col);
                            }
                            //  else
                            //       store_color_x(row, i, dec, BLACK);
                        }
                        else if (i == CALIBRATION_FIRST_BAR_CHROMA_INDEX - 1)
                        {
                            //   store_color_x(row, i, dec, BLACK);
                        }
                    }
                }
            }
            // draw already mapped colors
            else if (dec < current_sample)
            {
                int8_t col = calibration_data[row % 2][i % 4][dec];

                if (col == 0)
                    // mapped black
                    plotf(i, y, BLACK);

                else if (col > 15 || col < 0)
                {
                    // color not mapped
                    plotf(i, y, YELLOW);
                    _log_error_value(dec, i, row);
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
    if (row == 10)
        fine_tuned = false;

    if (row == 10)
    {
        // draw progress bar
        plotf(8 + (sample_frame / 8), 270 + (sample_frame % 8), BLUE);

        if (sample_frame == 2048)
        {
            c_step = SAVE;
            return;
        }
        sample_frame++;
    }

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (row > 66 && row < 266)
    {
        for (uint32_t i = 28; i < (CHROMA_LINE_LENGTH - 8); i++)
        {
            uint sample = chroma_buf[buf_seq][i];
            int dec = decode_intr(sample);

            int8_t col = calibration_data[row % 2][i % 4][dec];

            if (col == 0)
                // mapped black
                plotf(i, y, BLACK);

            else if (col > 15 || col < 0)
            // color not mapped
            {
                if (fine_tuned == false)
                {
                    fine_tuned = true;
                    col = fine_tune(dec, i, row);

                    if (col > 0)
                    {
                        plotf(i, y, col * 16 + 6);
                        update_mapping_diagram_c(dec, i, row, col * 16 + 6);
                    }
                    else
                    {
                        plotf(i, y, YELLOW);
                        _log_error_value(dec, i, row);
                    }
                }
                else
                {
                    plotf(i, y, YELLOW);
                    _log_error_value(dec, i, row);
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
    case SAVE:
        _save();
    }
}

#endif