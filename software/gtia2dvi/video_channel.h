#include "gtia_luma_ng.pio.h"
#include "gtia_chroma_ng.pio.h"
#include "gtia_palette.h"
#include "util/buttons.h"
#include "util/uart_log.h"

#ifndef VIDEO_CHANNEL_H
#define VIDEO_CHANNEL_H

#define GTIA_LUMA_SM 0
#define GTIA_COLOR_SM 1
#define PIO_LUMA pio1
#define LUMA_DMA_CHANNEL 10
#define CHROMA_DMA_CHANNEL 11
#define LUMA_LINE_LENGTH_BYTES 202

#define LUMA_START_OFFSET 12

static inline void calibrate_luma();

static inline void calibrate_chroma();

static inline void _wait_and_restart_dma();

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
}

static inline void _draw_luma_and_chroma_row(uint16_t row)
{
    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (y == scanline || y + 1 == scanline)
    {
        // don't modify line which is currently transferred to TDMS pipeline
        return;
    }

    uint32_t matched = 0;
    uint16_t *ptr = framebuf + y * FRAME_WIDTH + SCREEN_OFFSET_X;

    uint32_t *chroma_sample_ptr = chroma_buf[buf_seq];

    chroma_sample_ptr += 27;
    for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2); i++)
    {
        uint16_t c = luma_buf[i];
        uint16_t luma = c & 0xf;
        uint16_t col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;

        luma = (c >> 4) & 0xf;
        chroma_sample_ptr++;

        matched = match_color(*chroma_sample_ptr, row);
        if (matched == -1)
        {
            matched = match_color(*(ptr + 1), row);
        }

        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;

        luma = (c >> 8) & 0xf;
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;

        luma = (c >> 12) & 0xf;
        chroma_sample_ptr += ((i) & 0x1) + 1;

        matched = match_color(*chroma_sample_ptr, row);
        if (matched == -1)
        {
            matched = match_color(*(chroma_sample_ptr + 1), row);
        }
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;
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
        plot(x++, y, colors[pxl]);
        pxl = (c >> 4) & 0xf;
        plot(x++, y, colors[pxl]);
        pxl = (c >> 8) & 0xf;
        plot(x++, y, colors[pxl]);
        pxl = (c >> 12) & 0xf;
        plot(x++, y, colors[pxl]);
    }
}

void __not_in_flash_func(process_video_stream)()
{
    chroma_hwd_init(true);

    _setup_gtia_interface();

    uint16_t row;
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
        uart_log_flush();
        _wait_and_restart_dma();

        // swap buffer
        buf_seq = (buf_seq + 1) % 2;

        // pio returns line as negative number so we must correct this
        row = -luma_buf[0];
        if (row == 10)
        {
            // increase frame counter once per frame
            frame++;
        }
        if (row < FIRST_GTIA_ROW_TO_SHOW || row >= FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW)
        {
            // skip rows outside view port
            continue;
        }
        if (app_cfg.enableChroma)
        {
            _draw_luma_and_chroma_row(row);
        }
        else
        {
            _draw_luma_only_row(row);
        }
    }
}

static __attribute__((noinline)) void __not_in_flash_func(calibrate_luma)(bool (*handler)())
{
    _setup_gtia_interface();
    uint16_t row;
    bool notFinished = true;
    while (notFinished)
    {
        _wait_and_restart_dma();
        row = -luma_buf[0];

        if (row == 10)
        {
            notFinished = handler();
        }

        if (row < 60 || row > 159)
        {
            continue;
        }

        uint16_t x = 0;
        uint16_t y = row - 60 + 140;
        for (uint8_t i = 8; i < (LUMA_LINE_LENGTH_BYTES / 2); i++)
        {
            uint16_t c = luma_buf[i];
            uint8_t pxl = c & 0xf;
            plot(x++, y, colors[pxl]);
            pxl = (c >> 4) & 0xf;
            plot(x++, y, colors[pxl]);
            pxl = (c >> 8) & 0xf;
            plot(x++, y, colors[pxl]);
            pxl = (c >> 12) & 0xf;
            plot(x++, y, colors[pxl]);
        }
    }
}

void __not_in_flash_func(calibrate_chroma)()
{

    chroma_hwd_init();

    chroma_calibration_init();

    _setup_gtia_interface();

    uint16_t row;

    while (true)
    {
        _wait_and_restart_dma();

        buf_seq = (buf_seq + 1) % 2;

        row = -luma_buf[0];
        if (row == 10)
        {
            frame++;
        }
        if (row < FIRST_GTIA_ROW_TO_SHOW || row >= FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW)
        {
            continue;
        }
        chroma_calibrate(row);
    }
}

// useful for debug problems
void __not_in_flash("draw_line_number_histogram") draw_line_number_histogram()
{

    while (true)
    {
        while (!gtia_luma_ng_program_has_data(GTIA_LUMA_SM))
            tight_loop_contents();

        uint16_t line = gtia_luma_ng_program_get_data(GTIA_LUMA_SM);

        plot(line, luma_buf[line], YELLOW);
        luma_buf[line]++;

        if (luma_buf[line] > 200)
        {
            while (true)
                tight_loop_contents();
        }
    }
}

#endif