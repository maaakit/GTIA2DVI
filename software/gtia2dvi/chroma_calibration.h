#ifndef CHROMA_CALIBRATION_H
#define CHROMA_CALIBRATION_H

#include "gfx/gfx_text.h"
#include "chroma.h"
#include "chroma_map_diagram.h"

#define SAMPLING_FRAMES 3
#define STEP2_FRAMES 2048
#define STEP2_LOG_ENABLED true
#define CALIB_FINE_TUNE_THRESHOLD 2

#define ATARI_BASIC_ROW(y) ((y) - 69)
#define ATARI_BASIC_COLUMN(x) ((x) - app_cfg.chroma_calib_offset)

enum calib_step
{
    GEOMETRY_CHECK = 1,
    STEP1,
    STEP2,
    SAVE
};

enum calib_step c_step = GEOMETRY_CHECK;

uint16_t current_sample = 0;
uint32_t sample_frame = 0;

static bool err_draw = false;

int16_t color_counts[2][4][15];

static inline void store_color(int row, int x, int16_t decode, int8_t color_index)
{
    if (decode < 0)
        return;

    if (color_index > 15 || color_index < 0)
        return;

    if (color_index == 15)
        color_index = 1;

    if (calibration_data[row % 2][x & 0x3][decode] == -1)
        calibration_data[row % 2][x & 0x3][decode] = color_index;
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
    color_counts[row % 2][x & 0x3][color_index]++;
}

static inline void color_counts_process(int row, uint16_t current_sample)
{
    int mod_row = row % 2;
    int mod_x = (row / 2) & 0x3;
    int max_i = 0, max_cnt = 0, prev_i = 0, prev_cnt = 0;

    for (int i = 0; i < 15; i++)
    {
        if (color_counts[mod_row][mod_x][i] > max_cnt)
        {
            prev_i = max_i;
            prev_cnt = max_cnt;
            max_cnt = color_counts[mod_row][mod_x][i];
            max_i = i;
        }
    }
    if (max_i > 0)
    {
        store_color(mod_row, mod_x, current_sample, max_i);
        if (prev_i > 0)
            sprintf(buf, "s:%03X r:%d x:%d c:%d ~%d ALT:%d ~%d", current_sample, mod_row, mod_x, max_i, max_cnt, prev_i, prev_cnt);
        else
            sprintf(buf, "s:%03X r:%d x:%d c:%d ~%d", current_sample, mod_row, mod_x, max_i, max_cnt);
        uart_log_putln(buf);
    }
}

static inline void chroma_calibration_init()
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
    int f = DEC_F(dec);
    int a = DEC_A(dec);
    int b = DEC_B(dec);

    for (int aa = MAX(0, a - 1); aa < MIN(32, a + 1); aa++)
        for (int bb = MAX(0, b - 1); bb < MIN(32, b + 1); bb++)
        {
            int16_t dec2 = f | aa << 1 | bb << 6;
            int8_t neightbour = calibration_data[row % 2][x & 0x3][dec2];

            if (neightbour <= 0)
                continue;
            else
            {
                uint diff = MIN(abs(neightbour - color), 15 - abs(neightbour - color));
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
            store_color(row, x, dec, col);
            update_mapping_diagram_c(dec, x, row, col * 16 + 6);
#if STEP2_LOG_ENABLED
            sprintf(buf, "%03X %d.%d c:%d ADD", dec, x, row, col);
            uart_log_putln(buf);
#endif
            return col;
        }
        else
        {
#if STEP2_LOG_ENABLED
            sprintf(buf, "%03X %d.%d c:%d t:%d BAD", dec, x, row, col, treshold);
            uart_log_putln(buf);
#endif
            return -1;
        }
    }
    else
    {
        __breakpoint();
    }
}

static inline int8_t fine_tune(int16_t dec, uint16_t x, uint16_t row)
{
    int atari_column = x - app_cfg.chroma_calib_offset;

    // vertical boxes on the right side
    if (atari_column >= 152 && atari_column <= 160)
    {
        if (ATARI_BASIC_ROW(row) >= 12 && ATARI_BASIC_ROW(row) < 180)
        {
            int col = 1 + ((row - 80) / 12);
            int valid = update_if_valid(dec, x, row, col, CALIB_FINE_TUNE_THRESHOLD);
            return valid;
        }
        return -1;
    }
    else
    {
        // vertical lines of solid colors on top part of screen
        if (ATARI_BASIC_ROW(row) >= 0 && ATARI_BASIC_ROW(row) < 90)
        {

            uint8_t col = 1 + atari_column / 10;

            if (atari_column >= 0 && atari_column < 150)
            {
                if ((atari_column % 10) < 8)
                    return update_if_valid(dec, x, row, col, CALIB_FINE_TUNE_THRESHOLD);
                else
                    return 0;
            }
        }
        else if (ATARI_BASIC_ROW(row) >= 100 && ATARI_BASIC_ROW(row) < 192)
        {
            int px;
            if (ATARI_BASIC_COLUMN(x) >= 66)
                px = ATARI_BASIC_COLUMN(x) - 66;
            else
                px = ATARI_BASIC_COLUMN(x);

            if (px < 60)
            {
                if ((px & 0x3) < 2)
                {
                    // strip
                    uint8_t col = 1 + (px / 4);
                    return update_if_valid(dec, x, row, col, CALIB_FINE_TUNE_THRESHOLD);
                }
                else
                {
                    uint8_t col = 1 + ((ATARI_BASIC_ROW(row) - 100) / 6);
                    return update_if_valid(dec, x, row, col, CALIB_FINE_TUNE_THRESHOLD);
                }
            }
        }
    }
    return -1;
}

static inline void _log_error_value(int16_t dec, uint i, uint16_t row)
{
    if (err_draw == false)
    {
        err_draw = true;
        update_mapping_diagram_err(dec, i, row);
    }
}

static __noinline int8_t _calculate_chroma_phase_adjust()
{
    uint16_t dec_tmp[4];
    int8_t calib_data_adjustment = -1;
    for (int i = 0; i < 4; i++)
    {
        uint16_t dec0 = decode_intr(chroma_buf[buf_seq][i + 4]);
        sprintf(buf, " %04X %d.%d.%d ", dec0, DEC_F(dec0), DEC_A(dec0), DEC_B(dec0));
        uart_log_putln(buf);
        uart_log_flush();

        dec_tmp[i] = dec0;
    }
    for (int i = 0; i < 4; i++)

        if ((dec_tmp[i] & 1) == 0)
        {
            if (calib_data_adjustment == -1)
            {
                calib_data_adjustment = i;
            }
            else
            {
                uint prev_a = DEC_A(dec_tmp[calib_data_adjustment]);
                uint this_a = DEC_A(dec_tmp[i]);
                if (this_a > prev_a)
                {
                    calib_data_adjustment = i;
                }
            }
        }

    if (calib_data_adjustment == -1)
    {
        // assertion error expected only one sample 0.A.B
        uart_log_putln("unable to compute chroma phase adjustment");
        return -1;
    }

    sprintf(buf, "calculated adjustment: %d", calib_data_adjustment);
    uart_log_putln(buf);

    return calib_data_adjustment;
}

static __noinline void calibration_data_adjust(uint8_t adj)
{
    if (adj == 0)
        return;

    sprintf(buf, " applying chroma data phase shift adjustment: %d", adj);
    uart_log_putln(buf);

    uint8_t tmp[4];
    for (int j = 0; j < 2; j++)
        for (int k = 0; k < 2048; k++)
        {
            for (int i = 0; i < 4; i++)
                tmp[i] = calibration_data[j][i][k];
            for (int i = 0; i < 4; i++)
                calibration_data[j][(i + adj) & 0x3][k] = tmp[i];
        }
    uart_log_putln(" adjustmend aplied");
}

static __noinline void _save()
{
    int8_t neg_adj = (4 - chroma_phase_adjust) & 0x3;
    calibration_data_adjust(neg_adj);

    uart_log_putln("requesting calibration data save");

    app_cfg.enableChroma = true;
    set_post_boot_action(WRITE_CONFIG);
    set_post_boot_action(WRITE_PRESET);
    watchdog_enable(100, 1);

    while (true)
    {
        uart_log_flush_blocking();
        tight_loop_contents();
    }
}

static inline bool process_registered_chroma_data(uint16_t row)
{
    if (row >= 2 && row <= 9)
        color_counts_process(row, current_sample);
    if (row == 10)
    {
        if (current_sample < 0x7ff)
        {
            // update mapping diagram
            update_mapping_diagram(current_sample);
            color_counts_clear();
            int a, b;
            do
            {
                // draw progress bar
                plotf(8 + (current_sample / 8), 270 + (current_sample % 8), GREEN);

                current_sample++;
                // skip unexpected a and b values
                a = DEC_A(current_sample);
                b = DEC_B(current_sample);
            } while (a + b > 28);
            sample_frame = 0;
            return false;
        }
        else
        {
            c_step = STEP2;
            sample_frame = 0;

            uart_log_putln("entering calibration step 2");
            uart_log_flush_blocking();
        }
    }
    return true;
}

static inline void __not_in_flash_func(chroma_calibrate_step1)(uint16_t row)
{
    static bool processing = false;

    err_draw = false;

    if (row == 1)
    {
        sample_frame++;
        if (sample_frame == SAMPLING_FRAMES)
            processing = true;
    }

    if (processing == true)
        processing = process_registered_chroma_data(row);

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (row == 64)
    {
        // draw expected colors row
        for (int x = 0; x < 150; x++)
        {
            int col = 1 + (x / 10);
            if (col == 15)
                col = 1;

            if ((x % 10) < 8)
                plotf(x + app_cfg.chroma_calib_offset, y, col * 16 + 6);
        }
    }

    if (ATARI_BASIC_ROW(row) >= 0 && ATARI_BASIC_ROW(row) < 192)
    {
        for (uint32_t i = 30; i < (CHROMA_LINE_LENGTH - 10); i++)
        {
            uint sample = chroma_buf[buf_seq][i];
            int dec = decode_intr(sample);

            if (dec == current_sample && ATARI_BASIC_ROW(row) >= 0 && ATARI_BASIC_ROW(row) < 90)
            {
                // mark matching pixels
                plotf(i, y, row % 2 ? RED : MAGENTA);

                if (sample != 0)
                {
                    if (dec == current_sample)
                    {
                        int atari_column = i - app_cfg.chroma_calib_offset;
                        uint8_t col = 1 + atari_column / 10;
                        if (col == 15)
                            col = 1;

                        if (atari_column >= 0 && atari_column < 150)
                        {
                            if ((atari_column % 10) < 8)
                            {
                                if (col > 0 && col <= 15)
                                    color_counts_add(row, i, col);
                            }
                        }
                    }
                }
            }
            // draw already mapped colors
            else if (dec < current_sample)
            {
                int8_t col = calibration_data[row % 2][i & 0x3][dec];
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
    // map black color
    if (ATARI_BASIC_ROW(row) > 192 && ATARI_BASIC_ROW(row) < 200)
        for (int x = 32; x < 64; x++)
        {
            uint sample = chroma_buf[buf_seq][x];
            int dec = decode_intr(sample);
            if (sample == 0)
                store_color(row, x, dec, BLACK);
        }
}

static inline void __not_in_flash_func(chroma_calibrate_step2)(uint16_t row)
{
    static bool extended = false;
    static bool fine_tuned = false;

    err_draw = false;

    if (row == 10)
    {
        fine_tuned = false;
        // draw progress bar
        plotf(8 + (sample_frame / (STEP2_FRAMES / 2048) / 8), 270 + (sample_frame % 8), BLUE);

        if (sample_frame < STEP2_FRAMES)
        {
            if (extended == false && btn_is_down(BTN_B))
            {
                extended = true;
                uart_log_putln("extended calibration enabled");
            }
        }

        if (sample_frame > STEP2_FRAMES && (btn_is_down(BTN_B) || extended == false))
        {
            uart_log_putln("calibration finished");
            c_step = SAVE;
            return;
        }

        if (extended && (sample_frame & 0xf) == 0)
            gpio_xor_mask(1 << 25);

        sample_frame++;
    }

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (ATARI_BASIC_ROW(row) >= 0 && ATARI_BASIC_ROW(row) < 192)
    {
        if (ATARI_BASIC_ROW(row) < 90 || ATARI_BASIC_ROW(row) >= 100)
            if (chroma_buf[buf_seq][app_cfg.chroma_calib_offset - 1] != 0 || chroma_buf[buf_seq][app_cfg.chroma_calib_offset] == 0)
            {
                sprintf(buf, "bad row: %d", row);
                uart_log_putln(buf);
                return;
            }

        for (uint32_t i = 28; i < (CHROMA_LINE_LENGTH - 8); i++)
        {
            uint sample = chroma_buf[buf_seq][i];
            int dec = decode_intr(sample);

            int8_t color = calibration_data[row % 2][i & 0x3][dec];

            if (color == 0)
                // mapped black
                plotf(i, y, BLACK);

            else if (color > 15 || color < 0)
            // color not mapped
            {
                if (fine_tuned == false)
                {
                    fine_tuned = true;
                    color = fine_tune(dec, i, row);

                    if (color > 0)
                    {
                        plotf(i, y, color * 16 + 6);
                        update_mapping_diagram_c(dec, i, row, color * 16 + 6);
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
                plotf(i, y, color * 16 + 6);
        }
    }
}

static inline void calib_error(char *msg, char *uartmsg)
{
    uart_log_putln(msg);
    uart_log_putln(uartmsg);
    set_pos(100, 100);
    set_fore_col(YELLOW);
    put_text(msg);
    while (true)
    {
        tight_loop_contents();
    }
}

static inline void __not_in_flash_func(geometry_check)(uint16_t row)
{
    static int chroma_calib_offset_tmp = -1;

    if (row == 310)
    {
        sample_frame++;
        if (sample_frame == 2)
        {
            if (chroma_calib_offset_tmp == -1)
                calib_error("ERR BAD_GEO_02", "");
            else
            {
                sprintf(buf, "geometry setup: chroma_calib_offset found at: %d", chroma_calib_offset_tmp);
                uart_log_putln(buf);
                app_cfg.chroma_calib_offset = chroma_calib_offset_tmp;
                c_step = STEP1;
                return;
            }
        }
    }

    if (sample_frame == 1)
        if (ATARI_BASIC_ROW(row) >= 0 && ATARI_BASIC_ROW(row) < 192)
            if (ATARI_BASIC_ROW(row) < 90 || ATARI_BASIC_ROW(row) >= 100)
            {
                for (int i = 26; i < 36; i++)
                {
                    if (chroma_buf[buf_seq][i] != 0)
                    {
                        if (chroma_calib_offset_tmp == -1)
                        {
                            chroma_calib_offset_tmp = i;
                            return;
                        }
                        else
                        {
                            if (chroma_calib_offset_tmp == i) // found chroma data at expected offset
                                return;
                            else
                            {
                                sprintf(buf, "geometry error in row: %d, chroma date expected pos: %d but got at %d", row, chroma_calib_offset_tmp, i);
                                calib_error("ERR BAD_GEO_01", buf);
                            }
                        }
                    }
                }
                sprintf(buf, "geometry error in row: %d", row);
                calib_error("ERR BAD_GEO_03", buf);
            }
}

static inline void __not_in_flash_func(chroma_calibrate)(uint16_t row)
{
    switch (c_step)
    {
    case GEOMETRY_CHECK:
        geometry_check(row);
        break;
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

static inline void __not_in_flash_func(pot_adjust_row)(uint16_t row)
{
    static uint8_t pot_adjust_data[16 * 1024];

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;
    const int offset = app_cfg.chroma_calib_offset ? app_cfg.chroma_calib_offset : CALIBRATION_FIRST_BAR_CHROMA_INDEX;

    if (row > 70 && row < 155)
        for (int i = 0; i < 15; i++)
            for (int j = 0; j < 8; j++)
            {
                int x = i * 10 + j + offset;
                uint sample = chroma_buf[buf_seq][x];
                int dec = decode_intr(sample);
                uint8_t col = i == 14 ? 0 : i;

                int index = dec | ((row % 2) << 11) | ((x & 0x3) << 12);

                uint diff = abs(pot_adjust_data[index] - col);

                switch (diff)
                {
                case 0:
                    plotf(x, y, 2);
                    break;
                case 1:
                    plotf(x, y, 6);
                    pot_adjust_data[index] = col;
                    break;

                default:
                    plotf(x, y, YELLOW);
                    pot_adjust_data[index] = col;
                    break;
                }
            }
}

#endif