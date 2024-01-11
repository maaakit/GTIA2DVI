#include "gtia_luma_ng.pio.h"
#include "gtia_chroma_ng.pio.h"
#include "gtia_palette.h"

#define GTIA_LUMA_SM 0
#define GTIA_COLOR_SM 1
#define PIO_LUMA pio1
#define LUMA_DMA_CHANNEL 10
#define CHROMA_DMA_CHANNEL 11
#define LUMA_LINE_LENGTH_BYTES 202

uint16_t luma_buf[LUMA_LINE_LENGTH_BYTES / 2];

static inline void setup_hwd()
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

    uint offset_lm = pio_add_program(PIO_LUMA, &gtia_luma_ng_program);
    uint offset_ch = pio_add_program(pio1, &gtia_chroma_ng_program);
    gtia_chroma_ng_program_init(GTIA_COLOR_SM, offset_ch, CHROMA_LINE_LENGTH);
    gtia_luma_ng_program_init(PIO_LUMA, GTIA_LUMA_SM, offset_lm, LUMA_LINE_LENGTH_BYTES);
}

void __not_in_flash("video_stream") process_video_stream()
{
    chroma_init(true);

    setup_hwd();

    uint16_t row;
    plot(399, 239, GREEN);
    while (true)
    {
        // wait for both DMA channels
        dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);
        dma_channel_wait_for_finish_blocking(CHROMA_DMA_CHANNEL);
        // restart DMA channels
        dma_channel_set_write_addr(CHROMA_DMA_CHANNEL, &chroma_buf[buf_seq], true);
        dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);

        // swap buffer
        buf_seq = (buf_seq + 1) % 2;

        // pio returns line as egative number so we must correct this
        row = -luma_buf[0];
        if (row == 10)
        {
            // increase frame counter once per frame
            frame++;
        }
        if (row < 43 || row > 280)
        {
            // skip rows outside view port
            continue;
        }
        draw_luma_and_chroma(row);
    }
}

void __not_in_flash_func(calibrate_chroma)()
{
    chroma_init(false);

    setup_hwd();

    uint16_t row;
    plot(399, 239, GREEN);
    while (true)
    {
        dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);
        dma_channel_wait_for_finish_blocking(CHROMA_DMA_CHANNEL);
        dma_channel_set_write_addr(CHROMA_DMA_CHANNEL, &chroma_buf[buf_seq], true);
        dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);

        buf_seq = (buf_seq + 1) % 2;

        row = -luma_buf[0];
        if (row == 10)
        {
            frame++;
        }
        if (row < 43 || row > 280)
        {
            continue;
        }

#ifdef DRAW_LUMA
        uint16_t x = 0;
        uint16_t y = row - 43;
        for (uint8_t i = 8; i <= (LUMA_LINE_LENGTH_BYTES / 2); i++)
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

        if (frame == 1000 && row == 100)
        {
            sleep_us(1000);
            plot(393, 239, BLUE);
        }
#else
        if (!chroma_calibration_finished())
        {
            chroma_calibrate(row);
        }
        else
        {
            draw_luma_and_chroma(row);
        }

#endif
    }
}

inline void __not_in_flash_func(draw_luma_and_chroma)(uint16_t row)
{
    uint32_t y = row - 43;
    uint32_t matched = 0;
    uint16_t *ptr = framebuf + y * FRAME_WIDTH + 20;

    uint32_t *chroma_sample_ptr = chroma_buf[buf_seq];

    chroma_sample_ptr += 27;
    for (uint32_t i = 12; i <= (LUMA_LINE_LENGTH_BYTES / 2); i++)
    {
        uint16_t c = luma_buf[i];
        uint16_t luma = c & 0xf;
        uint16_t col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;

        luma = (c >> 4) & 0xf;
        chroma_sample_ptr++;
#ifndef IGNORE_CHROMA
        matched = match_color(*chroma_sample_ptr, row);
        if (matched == -1)
        {
            matched = match_color(*(ptr + 1), row);
        }
#endif
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;

        luma = (c >> 8) & 0xf;
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;

        luma = (c >> 12) & 0xf;
        chroma_sample_ptr += ((i) & 0x1) + 1;
#ifndef IGNORE_CHROMA
        matched = match_color(*chroma_sample_ptr, row);
        if (matched == -1)
        {
            matched = match_color(*(chroma_sample_ptr + 1), row);
        }
#endif
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *ptr++ = col565;
    }
}

// not used
void __not_in_flash("draw_luma_dma") draw_luma_dma()
{

    gpio_init_mask(INPUT_PINS_MASK);
    gpio_set_dir_in_masked(INPUT_PINS_MASK);

    dma_channel_config c = dma_channel_get_default_config(LUMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio1, GTIA_LUMA_SM, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_high_priority(&c, true);
    dma_channel_configure(LUMA_DMA_CHANNEL, &c, &luma_buf, &pio1->rxf[GTIA_LUMA_SM], (LUMA_LINE_LENGTH_BYTES + 4) / 4, false);

    dma_channel_config c2 = dma_channel_get_default_config(CHROMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
    channel_config_set_dreq(&c2, pio_get_dreq(pio1, GTIA_COLOR_SM, false));
    channel_config_set_read_increment(&c2, false);
    channel_config_set_write_increment(&c2, true);
    channel_config_set_high_priority(&c2, true);
    dma_channel_configure(CHROMA_DMA_CHANNEL, &c2, &chroma_buf, &pio1->rxf[GTIA_COLOR_SM], 32 / 2, false);

    uint offset_lm = pio_add_program(pio1, &gtia_luma_ng_program);
    uint offset_ch = pio_add_program(pio1, &gtia_chroma_ng_program);
    gtia_chroma_ng_program_init(GTIA_COLOR_SM, offset_ch, 0);
    gtia_luma_ng_program_init(pio1, GTIA_LUMA_SM, offset_lm, LUMA_LINE_LENGTH_BYTES);

    uint16_t row;
    plot(399, 239, GREEN);
    while (true)
    {
        dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);
        dma_channel_set_write_addr(CHROMA_DMA_CHANNEL, &chroma_buf, true);
        dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);

        row = -luma_buf[0];

        if (row < 43 || row > 283)
        {
            continue;
        }

        uint16_t x = 0;
        uint16_t y = row - 43;
        for (uint8_t i = 8; i <= (LUMA_LINE_LENGTH_BYTES / 2); i++)
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
        for (int q = 0; q < 32; q++)
        {
            framebuf[(row - 43) * FRAME_WIDTH + q] = chroma_buf[q];
        }
    }
}

// uswful for debug problems
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
