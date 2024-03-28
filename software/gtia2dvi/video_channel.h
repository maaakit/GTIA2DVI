#include "gtia_luma_ng.pio.h"
#include "gtia_pal.pio.h"
#include "gtia_color.pio.h"
#include "gtia_palette.h"
#include "util/buttons.h"
#include "util/uart_log.h"
#include "hardware/pio.h"

#include "chroma_calib.h"

#ifndef VIDEO_CHANNEL_H
#define VIDEO_CHANNEL_H

#define GTIA_PIO pio1

#define LUMA_DMA_CHANNEL 9
#define COLOR_DMA_CHANNEL 10
#define PAL_DMA_CHANNEL 11

#define LUMA_LINE_LENGTH_BYTES 202

#define LUMA_START_OFFSET 12

#define INPUT_PINS_MASK 0xff

char buf[80];



static inline void calibrate_luma();

void __attribute__((noinline)) __not_in_flash_func(calibrate_chroma)();

uint16_t luma_buf[LUMA_LINE_LENGTH_BYTES / 2];

static inline void __scratch_y("wait_and_restart_dma") _wait_and_restart_dma(uint16_t row)
{
#ifdef LED_WAIT_DMA
    gpio_put(LED_PIN, 1);
#endif
    // wait for both DMA channels
    dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);

    // set color delay for new line
    gtia_color_program_delay(delays[row % 2]);

    // restart each DMA channel
    dma_channel_set_write_addr(PAL_DMA_CHANNEL, &pal_buf[buf_seq], true);
    dma_channel_set_write_addr(COLOR_DMA_CHANNEL, &color_buf[buf_seq], true);
    dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);
#ifdef LED_WAIT_DMA
    gpio_put(LED_PIN, 0);
#endif
}

static uint mode = 2;
static bool force_dump = false;
#define MIN_COLOR 10

static inline void __not_in_flash_func(_setup_gtia_interface)()
{
    // initialize all GPIO data pins as input
    gpio_init_mask(INPUT_PINS_MASK);
    gpio_set_dir_in_masked(INPUT_PINS_MASK);

    // initialize DMA for luma data
    dma_channel_config c = dma_channel_get_default_config(LUMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(GTIA_PIO, GTIA_LUMA_SM, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(LUMA_DMA_CHANNEL, &c, &luma_buf, &GTIA_PIO->rxf[GTIA_LUMA_SM], (LUMA_LINE_LENGTH_BYTES + 2) / 4, false);

    // initialize DMA for PAL data
    dma_channel_config c2 = dma_channel_get_default_config(PAL_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
    channel_config_set_dreq(&c2, pio_get_dreq(GTIA_PIO, GTIA_PAL_SM, false));
    channel_config_set_read_increment(&c2, false);
    channel_config_set_write_increment(&c2, true);
    channel_config_set_high_priority(&c2, true);
    dma_channel_configure(PAL_DMA_CHANNEL, &c2, &pal_buf, &GTIA_PIO->rxf[GTIA_PAL_SM], CHROMA_SAMPLES, false);

    // initialize DMA for COLOR data
    dma_channel_config c3 = dma_channel_get_default_config(COLOR_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c3, DMA_SIZE_32);
    channel_config_set_dreq(&c3, pio_get_dreq(GTIA_PIO, GTIA_COLOR_SM, false));
    channel_config_set_read_increment(&c3, false);
    channel_config_set_write_increment(&c3, true);
    channel_config_set_high_priority(&c3, true);
    dma_channel_configure(COLOR_DMA_CHANNEL, &c3, &color_buf, &GTIA_PIO->rxf[GTIA_COLOR_SM], CHROMA_SAMPLES, false);

    // initialize each pio programs
    gtia_pal_v2_program_init(CHROMA_SAMPLES);
    gtia_color_program_init(CHROMA_SAMPLES);
    gtia_luma_ng_program_init(LUMA_LINE_LENGTH_BYTES);
}

static inline void _draw_positioning(uint16_t row)
{
    static inline uint32_t sums[2];
    static inline uint32_t counts[2];

    if (row == FIRST_GTIA_ROW_TO_SHOW)
    {
        if ((frame % 25) == 1)
        {
            sprintf(buf, "#0 %06d %05d", sums[0], counts[0]);
            UART_LOG_PUTLN(buf);
            sprintf(buf, "#1 %06d %05d", sums[1], counts[1]);
            UART_LOG_PUTLN(buf);
        }
        sums[0] = 0;
        sums[1] = 0;
        counts[0] = 0;
        counts[1] = 0;
    }
    else
    {

        for (uint x = 0; x < CHROMA_SAMPLES; x++)
        {
            uint32_t color = color_buf[buf_seq][x];
            if (popcount(color) < MIN_COLOR && popcount(color) > 0)
            {
                counts[row % 2]++;
                sums[row % 2] += popcount(color);
            }

            plotf(x + 10, row - FIRST_GTIA_ROW_TO_SHOW, popcount(color) < MIN_COLOR && popcount(color) > 0 ? YELLOW : popcount(color)); // color_buf[buf_seq][x]);
        }
    }
}

static uint32_t x_counts[2];
static uint32_t x_sums[2];

static __attribute__((noinline)) void __not_in_flash_func(_delays_calibration)(uint16_t row)
{
    if (row == FIRST_GTIA_ROW_TO_SHOW && (frame % 20 == 0))
    {
        sprintf(buf, "%d %d %d %d %d %d", x_counts[0], x_sums[0], x_counts[1], x_sums[1], delays[0], delays[1]);
        x_counts[0] = 0;
        x_sums[0] = 0;
        x_counts[1] = 0;
        x_sums[1] = 0;
        uart_log_putln(buf);
        // if ((frame % 100) == 0)
        // {
        //     delays[0] = (delays[0] + 1) % 64;
        //     delays[1] = delays[0];
        // }
    }

    uint16_t x = SCREEN_OFFSET_X;
    uint16_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    uint col_offset = 26;

    for (uint x = 0; x < CHROMA_SAMPLES; x++)
    {
        uint bits = POPCNT(color_buf[buf_seq][x]);
        uint16_t col;
        if (bits == 0)
        {
            col = BLACK;
        }
        else if (bits > 0 && bits < 13)
        {
            col = YELLOW;
            x_counts[row % 2]++;
            x_sums[row % 2] += bits;
        }
        else if (bits > 12 && bits < 24)
        {
            col = GRAY(10);
        }
        else
        {
            col = GREEN;
        }
        plotf(x + 10, row - FIRST_GTIA_ROW_TO_SHOW, col);
    }
}

static inline void _draw_luma_mixed_with_chroma_row(uint16_t row)
{
    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (y % 3 == 0)
    {
        uint16_t x = 7; // SCREEN_OFFSET_X;
        //   uint16_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;
        for (uint16_t i = 0; i < (LUMA_LINE_LENGTH_BYTES / 2); i++)
        {
            uint16_t c = luma_buf[i];
            uint8_t pxl = c & 0xf;
            plotf(x++, y, colors[pxl]);
            // pxl = (c >> 4) & 0xf;
            // plot(x++, y, colors[pxl]);
            pxl = (c >> 8) & 0xf;
            plotf(x++, y, colors[pxl]);
            // pxl = (c >> 12) & 0xf;
            // plot(x++, y, colors[pxl]);

            if (i % 2 == 1)
            {
                plotf(x, y, BLACK);
                x++;
            }
        }
        for (; x < 320; x++)
        {
            plotf(x, y, BLACK);
        }
    }
    else
    {
        for (uint x = 0; x < CHROMA_SAMPLES - 0; x++)
        {
            uint16_t sample = decode_color(x, buf_seq);
            uint8_t color = chroma_table[sample][row % 2];
            plotf(x + 10, row - FIRST_GTIA_ROW_TO_SHOW, color == -1 ? YELLOW : gtia_palette[color * 16 + 4]); // color_buf[buf_seq][x]);
        }
    }
}

#ifdef DUMP_PIXEL_FEATURE_ENABLED
uint16_t pointer_x = 0, pointer_y = 0;
char cmd = 0;

static void __attribute__((noinline)) dump_pointer_data(uint16_t row)
{
    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;
    uint16_t *pixel_ptr = framebuf + y * FRAME_WIDTH + SCREEN_OFFSET_X;

    uint16_t *pixel_catch_ptr = pixel_ptr + pointer_x - LUMA_START_OFFSET - 2;

    uint32_t *color_ptr = color_buf[buf_seq];
    uint32_t *pal_ptr = pal_buf[buf_seq];

    color_ptr += 27;
    pal_ptr += 27;

    for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2) - 2; i++)
    {
        uint32_t c = luma_buf[i];
        uint32_t luma = c & 0xf;
        uint16_t col565 = BLACK; // matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        if (pixel_ptr == pixel_catch_ptr)
        {
            sprintf(buf, "0 pos: %03d,%03d lu: %02d i: %03d co: %08x pa: %08x ne: %08x -> %d/%d",
                    pointer_x, pointer_y, luma, pal_ptr - pal_buf[buf_seq], *color_ptr, *pal_ptr, *(color_ptr + 1), match_color(*color_ptr, *pal_ptr, row), match_color(*(color_ptr + 1), *(pal_ptr + 1), row));
            *pixel_ptr++ = WHITE;
        }
        else
            *pixel_ptr++ = col565;

        luma = (c >> 4) & 0xf;
        color_ptr++;
        pal_ptr++;

        // matched = match_color(*color_ptr, *pal_ptr, row);

        col565 = BLACK; // matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        if (pixel_ptr == pixel_catch_ptr)
        {
            sprintf(buf, "1 pos: %03d,%03d lu: %02d i: %03d co: %08x pa: %08x ne: %08x -> %d/%d",
                    pointer_x, pointer_y, luma, pal_ptr - pal_buf[buf_seq], *color_ptr, *pal_ptr, *(color_ptr + 1), match_color(*color_ptr, *pal_ptr, row), match_color(*(color_ptr + 1), *(pal_ptr + 1), row));
            *pixel_ptr++ = WHITE;
        }
        else
            *pixel_ptr++ = col565;

        luma = (c >> 8) & 0xf;
        col565 = BLACK; // matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        if (pixel_ptr == pixel_catch_ptr)
        {
            sprintf(buf, "2 pos: %03d,%03d lu: %02d i: %03d co: %08x pa: %08x ne: %08x -> %d/%d",
                    pointer_x, pointer_y, luma, pal_ptr - pal_buf[buf_seq], *color_ptr, *pal_ptr, *(color_ptr + 1), match_color(*color_ptr, *pal_ptr, row), match_color(*(color_ptr + 1), *(pal_ptr + 1), row));
            *pixel_ptr++ = WHITE;
        }
        else
            *pixel_ptr++ = col565;

        luma = (c >> 12) & 0xf;
        color_ptr += ((i) & 0x1) + 1;
        pal_ptr += ((i) & 0x1) + 1;

        // matched = match_color(*color_ptr, *pal_ptr, row);

        col565 = BLACK; // matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        if (pixel_ptr == pixel_catch_ptr)
        {
            sprintf(buf, "3 pos: %03d,%03d lu: %02d i: %03d co: %08x pa: %08x ne: %08x -> %d/%d",
                    pointer_x, pointer_y, luma, pal_ptr - pal_buf[buf_seq], *color_ptr, *pal_ptr, *(color_ptr + 1), match_color(*color_ptr, *pal_ptr, row), match_color(*(color_ptr + 1), *(pal_ptr + 1), row));
            *pixel_ptr++ = WHITE;
        }
        else
            *pixel_ptr++ = col565;
    }
    uart_log_putln(buf);
    cmd = 0;
}
#endif

static inline void _draw_luma_and_chroma_row(uint16_t row)
{

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (y == scanline || y + 1 == scanline)
    {
        // don't modify line which is currently transferred to TDMS pipeline
        return;
    }

    int32_t matched = 0;
    uint16_t *pixel_ptr = framebuf + y * FRAME_WIDTH + SCREEN_OFFSET_X;

    // uint32_t *chroma_sample_ptr = chroma_buf[buf_seq];

    uint32_t *color_ptr = color_buf[buf_seq];
    uint32_t *pal_ptr = pal_buf[buf_seq];

    color_ptr += 27;
    pal_ptr += 27;

#ifdef DUMP_PIXEL_FEATURE_ENABLED
    if (cmd && y == pointer_y)
    {
        dump_pointer_data(row);
    }
    else
    {
#endif    
        for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2) - 2; i++)
        {
            uint32_t c = luma_buf[i];
            uint32_t luma = c & 0xf;
            uint16_t col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
            *pixel_ptr++ = col565;

            luma = (c >> 4) & 0xf;
            color_ptr++;
            pal_ptr++;

            matched = match_color(*color_ptr, *pal_ptr, row);

            col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
            *pixel_ptr++ = col565;

            luma = (c >> 8) & 0xf;
            col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
            *pixel_ptr++ = col565;

            luma = (c >> 12) & 0xf;
            color_ptr += ((i) & 0x1) + 1;
            pal_ptr += ((i) & 0x1) + 1;

            matched = match_color(*color_ptr, *pal_ptr, row);

            col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
            *pixel_ptr++ = col565;
        }
#ifdef DUMP_PIXEL_FEATURE_ENABLED    
    }
    if (pointer_x != 0 && pointer_y != 0)
    {
        plotf(pointer_x, pointer_y, RED);
    }
#endif    
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

void static handleButtons()
{
    enum BtnEvent btn = btn_last_event();
    if (btn == BTN_A_SHORT && preset_loaded)
    {
        mode = (mode + 1) % 2;
        if (mode == 0)
        {
            app_cfg.enableChroma = true; //! app_cfg.enableChroma;
        }
        if (mode == 1)
        {
            app_cfg.enableChroma = false; //! app_cfg.enableChroma;
        }
        sprintf(buf, "m: %d", mode);
        UART_LOG_PUTLN(buf);

        if (btn_is_down(BTN_B))
        {
            force_dump = true;
        }

        // UART_LOG_PUTLN("BTN_A");
    }
    if (btn == BTN_B_SHORT)
    {
        delays[0] = (delays[0] + 1) % 64;
        delays[1] = delays[0];
    }
}

void __attribute__((noinline)) __not_in_flash_func(handle_uart_rx)()
{
    buf[0] = 0;
    char c = get_char();
    if (c != 0)
    {
        switch (c)
        {
        case 'z':
            delays[0] = (delays[0] - 1) % 64;
            sprintf(buf, "%d %d", delays[0], delays[1]);
            break;
        case 'x':
            delays[0] = (delays[0] + 1) % 64;
            sprintf(buf, "%d %d", delays[0], delays[1]);
            break;
        case 'c':
            delays[1] = (delays[1] - 1) % 64;
            sprintf(buf, "%d %d", delays[0], delays[1]);
            break;
        case 'v':
            delays[1] = (delays[1] + 1) % 64;
            sprintf(buf, "%d %d", delays[0], delays[1]);
            break;
#ifdef DUMP_PIXEL_FEATURE_ENABLED            
        case '8':
            pointer_y = (pointer_y - 1) % 288;
            break;
        case '2':
            pointer_y = (pointer_y + 1) % 288;
            break;
        case '4':
            pointer_x = (pointer_x - 1) % 360;
            break;
        case '6':
            pointer_x = (pointer_x + 1) % 360;
            break;
        case '5':
            cmd = 'Q';
            break;
#endif
        default:
            sprintf(buf, "chr: %d", c);
        }
        if (buf[0] != 0)
            uart_log_putln(buf);
    }
}

void __not_in_flash_func(process_video_stream)()
{
    _setup_gtia_interface();

    uint16_t row = 0;
    while (true)
    {
        if (row == 2)
        {
            handleButtons();
        }

        uart_log_flush();
        _wait_and_restart_dma(row);
        if (row == 1)
        {
            handle_uart_rx();
        }

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
            _delays_calibration(row);
            // _draw_luma_mixed_with_chroma_row(row);
            // _draw_luma_only_row(row);
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
        _wait_and_restart_dma(row);
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

void __attribute__((noinline)) __scratch_y("calibrate_chroma") calibrate_chroma()
{
    UART_LOG_PUTLN("chromas calibration");
    _setup_gtia_interface();
    //   uint burst;
    uint16_t row = -1;

    calib_init();

    while (true)
    {

        // if (row == 4)
        // {
        //     handleButtons();
        // }
        UART_LOG_FLUSH();

        _wait_and_restart_dma(row);

        // swap buffer
        buf_seq = (buf_seq + 1) % 2;

        // pio returns line as negative number so change sign
        row = -luma_buf[0];

        if (row == 4)
        {
            // increase frame counter once per frame
            frame++;
        }

        if (row < FIRST_GTIA_ROW_TO_SHOW || row >= FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW)
        {
            // if (calib_finished == false)
            calib_handle(row);
        }
        else
        {
            calib_redraw(row, buf_seq);
        }
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