#include "gtia_luma_ng.pio.h"
#include "gtia_chroma_ng.pio.h"
#include "gtia_palette.h"
#include "util/buttons.h"
#include "util/uart_log.h"
#include "chroma_calibration.h"
#include "burst.h"

#ifndef VIDEO_CHANNEL_H
#define VIDEO_CHANNEL_H

#define GTIA_LUMA_SM 0
#define GTIA_COLOR_SM 1
#define PIO_LUMA pio1
#define LUMA_DMA_CHANNEL 10
#define CHROMA_DMA_CHANNEL 11
#define LUMA_LINE_LENGTH_BYTES 202

#define LUMA_START_OFFSET 12

uint16_t luma_buf[LUMA_LINE_LENGTH_BYTES / 2];

static inline void _setup_gtia_interface()
{
    gpio_init_mask(INPUT_PINS_MASK);
    gpio_set_dir_in_masked(INPUT_PINS_MASK);

    dma_channel_config c = dma_channel_get_default_config(LUMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(PIO_LUMA, GTIA_LUMA_SM, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(LUMA_DMA_CHANNEL, &c, &luma_buf, &PIO_LUMA->rxf[GTIA_LUMA_SM], (LUMA_LINE_LENGTH_BYTES + 2) / 4, false);

    dma_channel_config c2 = dma_channel_get_default_config(CHROMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
    channel_config_set_dreq(&c2, pio_get_dreq(pio1, GTIA_COLOR_SM, false));
    channel_config_set_read_increment(&c2, false);
    channel_config_set_write_increment(&c2, true);
    dma_channel_configure(CHROMA_DMA_CHANNEL, &c2, &chroma_buf, &pio1->rxf[GTIA_COLOR_SM], CHROMA_LINE_LENGTH, false);

    pio_sm_restart(PIO_LUMA, GTIA_LUMA_SM);
    pio_sm_restart(pio1, GTIA_COLOR_SM);
    uint offset_lm = pio_add_program(PIO_LUMA, &gtia_luma_ng_program);
    uint offset_ch = pio_add_program(pio1, &gtia_chroma_ng_program);
    gtia_chroma_ng_program_init(GTIA_COLOR_SM, offset_ch, CHROMA_LINE_LENGTH);
    gtia_luma_ng_program_init(PIO_LUMA, GTIA_LUMA_SM, offset_lm, LUMA_LINE_LENGTH_BYTES);
}

static inline void _wait_and_restart_dma()
{
    // wait for both DMA channels
    dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);
    dma_channel_wait_for_finish_blocking(CHROMA_DMA_CHANNEL);
    // restart DMA channels
    dma_channel_set_write_addr(CHROMA_DMA_CHANNEL, &chroma_buf[buf_seq], true);
    dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);

    buf_seq = (buf_seq + 1) % 2;
}

#ifdef DUMP_PIXEL_FEATURE_ENABLED
uint16_t pxl_dump_pos_x = 0, pxl_dump_pos_y = 0;
char cmd = 0;

#define DUMP_PIXEL_DATA_IF_MATCH(X)                                                                                        \
    if (pixel_ptr == pixel_catch_ptr)                                                                                      \
    sprintf(buf, "%1d pos: %03d,%03d lu: %02d i: %03d pa: %08x co: %08x / %08x -> %d/%d [%d,%d]",                          \
            (X) / 4, pxl_dump_pos_x, pxl_dump_pos_y, _get_current_luma(INTERP0_LUMA_SHIFT(X)), pal_ptr - pal_buf[buf_seq], \
            *pal_ptr, *color_ptr, *(color_ptr + 1), match_color(*color_ptr, *pal_ptr, row), match_color(*(color_ptr + 1), *(pal_ptr + 1), row), decode(*color_ptr, *pal_ptr) & 0x1f, (decode(*color_ptr, *pal_ptr) >> 5) & 0x1f)

/*
    dumps to UART console debug data describing single pixel pointed by user.
    pointed pixel is display as red dot that can be moved with UART console with numeric keys:
    4 for left direction
    8 for up direction
    6 for right direction
    2 for down
    5 triggers data to be writen to UART
*/
static void __not_in_flash_func(_locate_and_dump_pixel_data)(uint16_t row)
{
    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;
    uint16_t *pixel_ptr = framebuf + y * FRAME_WIDTH + SCREEN_OFFSET_X;
    uint16_t *pixel_catch_ptr = pixel_ptr + pxl_dump_pos_x - LUMA_START_OFFSET - 2;
    uint32_t *color_ptr = color_buf[buf_seq];
    uint32_t *pal_ptr = pal_buf[buf_seq];

    color_ptr += CHROMA_START_OFFSET;
    pal_ptr += CHROMA_START_OFFSET;

    for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2) - LUMA_WORDS_TO_SKIP; i++)
    {
        uint32_t luma_4px = luma_buf[i];
        interp0->accum[1] = luma_4px << 1;

        // first luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(0);
        *pixel_ptr++ = BLACK;

        color_ptr++;
        pal_ptr++;

        // second luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(4);
        *pixel_ptr++ = BLACK;

        // third luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(8);
        *pixel_ptr++ = BLACK;

        // move to next chroma pixel
        color_ptr++;
        pal_ptr++;

        // forth luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(12);
        *pixel_ptr++ = BLACK;
    }
    uart_log_putln(buf);
    cmd = 0;
}
#endif

static inline int8_t match_color(int chroma_pos, int row)
{
    uint32_t sample = chroma_buf[buf_seq][chroma_pos];
    int dec = decode_intr(sample);
    int col = calibration_data[row % 2][chroma_pos % 4][dec];
    if (col < 0)
        col = 0;
    return col;
}

static inline void _draw_luma_and_chroma_row(uint16_t row)
{
    uint16_t x = SCREEN_OFFSET_X;
    uint16_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (y == scanline || y + 1 == scanline)
    {
        // don't modify line which is currently transferred to TDMS pipeline
        return;
    }

    uint cpos = 22;
    int col = 0;

    for (uint16_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2); i++)
    {
        uint16_t c = luma_buf[i];
        uint8_t luma = c & 0xf;
        plotf(x++, y, col * 16 + luma);

        col = match_color(cpos++, row);
        luma = (c >> 4) & 0xf;
        plotf(x++, y, col * 16 + luma);

        luma = (c >> 8) & 0xf;
        plotf(x++, y, col * 16 + luma);

        col = match_color(cpos++, row);
        luma = (c >> 12) & 0xf;
        plotf(x++, y, col * 16 + luma);
    }
}

static inline void _draw_luma_only_row(uint16_t row)
{
    uint16_t x = SCREEN_OFFSET_X;
    uint16_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;
    for (uint16_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2); i++)
    {
        uint16_t c = luma_buf[i];
        uint8_t pxl = c & 0xf;
        plot(x++, y, pxl);
        pxl = (c >> 4) & 0xf;
        plot(x++, y, pxl);
        pxl = (c >> 8) & 0xf;
        plot(x++, y, pxl);
        pxl = (c >> 12) & 0xf;
        plot(x++, y, pxl);
    }
}

static inline void _prepare_calibration_data()
{
    int16_t row;
    // wait at least one frame (for reset row counter)
    for (int i = 0; i < 1000; i++)
    {
        _wait_and_restart_dma();
    }

    // wait for even row
    do
    {
        _wait_and_restart_dma();
        row = -luma_buf[0];
    } while (row % 2);

    // print starting sequence
    sprintf(buf, "STARTING SEQ row: %d", row);
    uart_log_putln(buf);

    int8_t calib_data_adjustment = _calculate_chroma_phase_adjust();

    chroma_phase_adjust = calib_data_adjustment;

}

void __not_in_flash_func(process_video_stream)()
{
    uint16_t row;

    chroma_hwd_init(true);

    _setup_gtia_interface();

    _prepare_calibration_data();

    calibration_data_adjust(chroma_phase_adjust);

    while (true)
    {
        enum BtnEvent btn = btn_last_event();

        if (btn == BTN_A_SHORT && preset_loaded)
        {
            app_cfg.enableChroma = !app_cfg.enableChroma;
            uart_log_putln("BTN_A");
        }
        if (btn == BTN_B_SHORT)
        {
            uart_log_putln("BTN_B");
        }

        _wait_and_restart_dma();
        // pio returns line as negative number so we must correct this
        row = -luma_buf[0];

        if (row == 8 || row == 9)
        {
            burst_analyze(row);
        }

        if (row == 10)
        {
            // increase frame counter once per frame
            frame_count++;
        }
        if (row < FIRST_GTIA_ROW_TO_SHOW || row >= FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW)
        {
            // skip rows outside view port
            continue;
        }
#ifdef DUMP_PIXEL_FEATURE_ENABLED
        if (cmd && y == pxl_dump_pos_y)
        {
            _locate_and_dump_pixel_data(row);
        }
        else
        {
#endif
            if (!btn_is_down(BTN_B))
            {
                if (app_cfg.enableChroma)
                    _draw_luma_and_chroma_row(row);
                else
                    _draw_luma_only_row(row);
            }
#ifdef DUMP_PIXEL_FEATURE_ENABLED
        }
        if (pxl_dump_pos_x != 0 && pxl_dump_pos_y != 0)
        {
            plotf(pxl_dump_pos_x, pxl_dump_pos_y, RED);
        }
#endif
    }
}

void __not_in_flash_func(calibrate_chroma)()
{
    uart_log_putln("starting chroma calibration");
    chroma_hwd_init();

    chroma_calibration_init();

    _setup_gtia_interface();

    _prepare_calibration_data();

    uint16_t row;

    while (true)
    {
        _wait_and_restart_dma();

        row = -luma_buf[0];
        if (row == 10)
        {
            frame_count++;
        }

        chroma_calibrate(row);
    }
}

#endif