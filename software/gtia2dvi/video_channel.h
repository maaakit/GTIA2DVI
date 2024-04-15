#include "gtia_luma_ng.pio.h"
#include "gtia_pal.pio.h"
#include "gtia_color.pio.h"
#include "gtia_palette.h"
#include "util/buttons.h"
#include "util/uart_log.h"
#include "hardware/pio.h"

#include "chroma_calib.h"
#include "color_burst.h"

#ifndef VIDEO_CHANNEL_H
#define VIDEO_CHANNEL_H

// hardware configuration
#define GTIA_PIO pio1
#define LUMA_DMA_CHANNEL 9
#define COLOR_DMA_CHANNEL 10
#define PAL_DMA_CHANNEL 11
#define INPUT_PINS_MASK 0xff

// row decoding logic configuration
#define LUMA_LINE_LENGTH_BYTES 202
#define LUMA_START_OFFSET 12
#define LUMA_WORDS_TO_SKIP (2+12)

#define INTERP0_LUMA_SHIFT(x) (0x00001020 + (x))

#define PTRN 0b1111111011111011

static inline void calibrate_luma();

void __attribute__((noinline)) __not_in_flash_func(calibrate_chroma)();

uint16_t __scratch_x("luma_buf") luma_buf[LUMA_LINE_LENGTH_BYTES / 2];

/*
    this function waits until the data transfer of the line is completed and then initiates the transfer of the next one.
*/
static inline void _wait_and_restart_dma(uint16_t row)
{
#ifdef LED_WAIT_DMA
    gpio_put(LED_PIN, 1);
#endif
    // wait for both DMA channels
    dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);

    // set color delay for new line
    gtia_color_program_delay(delays[row % 2]);

    // restart each DMA channel and set valid destination adress according to current bufer
    dma_channel_set_write_addr(PAL_DMA_CHANNEL, &pal_buf[buf_seq], true);
    dma_channel_set_write_addr(COLOR_DMA_CHANNEL, &color_buf[buf_seq], true);
    dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);
#ifdef LED_WAIT_DMA
    gpio_put(LED_PIN, 0);
#endif
}

/*
    setups all RP2040 hardware required to process GTIA data including:
    * GPIO input pins
    * DMA channels
    * PIO state machines
*/
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

/*
  calculates RGB565 value of GTIA color based on luma and color values with interpolator
*/
static inline uint16_t _gtia_color_565(uint32_t luma_shift)
{
    // update luma shift for interpolator for selecting right luma pixel
    interp0->ctrl[1] = luma_shift;
    // read pointer to rgb565 color from gtia_palette array calculated for color and luma by interp0
    uint16_t *color_location = interp0->peek[2];
    // returns rgb565 color value
    return *color_location;
}

#ifdef DUMP_PIXEL_FEATURE_ENABLED
uint16_t pxl_dump_pos_x = 0, pxl_dump_pos_y = 0;
char cmd = 0;

#define DUMP_PIXEL_DATA_IF_MATCH(X)                                                       \
    if (pixel_ptr == pixel_catch_ptr)                                                     \
    sprintf(buf, "%1d pos: %03d,%03d lu: %02d i: %03d pa: %08x co: %08x / %08x -> %d/%d [%d,%d]", \
            (X) / 4, pxl_dump_pos_x, pxl_dump_pos_y, _get_current_luma(INTERP0_LUMA_SHIFT(X)), pal_ptr - pal_buf[buf_seq], \
            *pal_ptr, *color_ptr, *(color_ptr + 1), match_color(*color_ptr, *pal_ptr, row), match_color(*(color_ptr + 1), \
            *(pal_ptr + 1), row ), decode(*color_ptr, *pal_ptr) & 0x1f, (decode(*color_ptr, *pal_ptr) >> 5 ) & 0x1f  )

/*
    gets current pixel luma value from interpolator
*/
static inline uint16_t _get_current_luma(uint32_t luma_shift)
{
    // update luma shift for interpolator for selecting right luma pixel
    interp0->ctrl[1] = luma_shift;
    // returns luma value for current pixel
    return interp0->peek[1] >> 1;
}



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
    uint16_t *pixel_catch_ptr = pixel_ptr + pxl_dump_pos_x - LUMA_START_OFFSET;
    uint32_t *color_ptr = color_buf[buf_seq];
    uint32_t *pal_ptr = pal_buf[buf_seq];

    color_ptr += 27;
    pal_ptr += 27;
    uint32_t pattern = PTRN;

    for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2) - LUMA_WORDS_TO_SKIP; i++)
    {
        uint32_t luma_4px = luma_buf[i];
        interp0->accum[1] = luma_4px << 1;

        // first luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(0);
        *pixel_ptr++ = BLACK;

        // calculate and move to next chroma pixel
        if (pattern & 1)
        {
            color_ptr++;
            pal_ptr++;
        }
        else
        {
            color_ptr += 2;
            pal_ptr += 2;
        }
        pattern = pattern >> 2;
        if (pattern == 0)
        {
            pattern = PTRN;
        }

        // second luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(4);
        *pixel_ptr++ = BLACK;

        // third luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(8);
        *pixel_ptr++ = BLACK;

        // move to next chroma pixel
        // color_ptr++;
        // pal_ptr++;
        // calculate and move to next chroma pixel
        if (pattern & 1)
        {
            color_ptr++;
            pal_ptr++;
        }
        else
        {
            color_ptr += 2;
            pal_ptr += 2;
        }
        pattern = pattern >> 2;
        if (pattern == 0)
        {
            pattern = PTRN;
        }

        // forth luma (hires) pixel
        DUMP_PIXEL_DATA_IF_MATCH(12);
        *pixel_ptr++ = BLACK;
    }
    uart_log_putln(buf);
    cmd = 0;
}
#endif
/*
    A highly time-critical function. Processing data for each line must complete before starting the data stream for the next line
*/
static void __attribute__((noinline)) __scratch_y("_draw_luma_and_chroma_row") _draw_luma_and_chroma_row(uint16_t row)
{

    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;

    if (y == scanline || y + 1 == scanline)
    {
        // don't modify line which is currently transferred to TDMS pipeline
        return;
    }

    uint8_t matched = 0;
    interp0->accum[0] = 0;
    uint16_t *pixel_ptr = framebuf + y * FRAME_WIDTH + SCREEN_OFFSET_X;
    uint32_t *color_ptr = color_buf[buf_seq];
    uint32_t *pal_ptr = pal_buf[buf_seq];

    color_ptr += 27;
    pal_ptr += 27;
    uint32_t pattern = PTRN;

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
            for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2) - LUMA_WORDS_TO_SKIP; i++)
            {
                uint32_t luma_4px = luma_buf[i];
                interp0->accum[1] = luma_4px << 1;

                // first luma (hires) pixel
                *pixel_ptr++ = _gtia_color_565(INTERP0_LUMA_SHIFT(0));

                // calculate and move to next chroma pixel
                if (pattern & 1)
                {
                    color_ptr++;
                    pal_ptr++;
                }
                else
                {
                    color_ptr += 2;
                    pal_ptr += 2;
                }
                pattern = pattern >> 2;
                if (pattern == 0)
                {
                    pattern = PTRN;
                }

                // second luma (hires) pixel
                matched = match_color(*color_ptr, *pal_ptr, row);
                interp0->accum[0] = matched << 5;
                *pixel_ptr++ = _gtia_color_565(INTERP0_LUMA_SHIFT(4));

                // third luma (hires) pixel
                *pixel_ptr++ = _gtia_color_565(INTERP0_LUMA_SHIFT(8));

                // move to next chroma pixel
                // color_ptr++;
                // pal_ptr++;
                if (pattern & 1)
                {
                    color_ptr++;
                    pal_ptr++;
                }
                else
                {
                    color_ptr += 2;
                    pal_ptr += 2;
                }
                pattern = pattern >> 2;
                if (pattern == 0)
                {
                    pattern = PTRN;
                }


                // forth luma (hires) pixel
                matched = match_color(*color_ptr, *pal_ptr, row);
                interp0->accum[0] = matched << 5;
                *pixel_ptr++ = _gtia_color_565(INTERP0_LUMA_SHIFT(12));
            }
        }
#ifdef DUMP_PIXEL_FEATURE_ENABLED
    }
    if (pxl_dump_pos_x != 0 && pxl_dump_pos_y != 0)
    {
        plotf(pxl_dump_pos_x, pxl_dump_pos_y, RED);
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

void static __attribute__((noinline)) __not_in_flash_func(_handle_buttons)()
{
    enum BtnEvent btn = btn_last_event();
    if (btn == BTN_A_SHORT && preset_loaded)
    {
        app_cfg.enableChroma = !app_cfg.enableChroma;
    }
    if (btn == BTN_B_SHORT)
    {
    }
}

void static __attribute__((noinline)) __not_in_flash_func(_handle_uart_rx)()
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
            pxl_dump_pos_y = (pxl_dump_pos_y - 1) % 288;
            break;
        case '2':
            pxl_dump_pos_y = (pxl_dump_pos_y + 1) % 288;
            break;
        case '4':
            pxl_dump_pos_x = (pxl_dump_pos_x - 1) % 360;
            break;
        case '6':
            pxl_dump_pos_x = (pxl_dump_pos_x + 1) % 360;
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

/*
    The main function handling the decoding of incoming data from GTIA and updating the frame buffer for DVI.
    It is highly critical in terms of execution time
*/
void __scratch_y("process_video_stream") process_video_stream()
{
    _setup_gtia_interface();
    gtia_color_trans_table_init();

    interp_config cfg0 = interp_default_config();
    interp_config cfg1 = interp_default_config();
    interp_set_config(interp0, 0, &cfg0);
    interp_config_set_mask(&cfg1, 1, 4);
    interp_set_config(interp0, 1, &cfg1);
    interp0->base[2] = &gtia_palette;

    uint16_t row = 0;
    while (true)
    {

        if (row == 1)
        {
            _handle_buttons();
        }

        uart_log_flush();
        _wait_and_restart_dma(row);

        if (row == 2)
        {
            _handle_uart_rx();
        }
        // pio returns line as negative number so we must correct this
        row = -luma_buf[0];

        // swap buffers
        buf_seq = (buf_seq + 1) % 2;

        if (row == 5 || row ==6 ) 
        {
            color_burst_process(row);
        }

        if (row == 5)
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
            _draw_luma_and_chroma_row(row);
        else
            _draw_luma_only_row(row);
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
    UART_LOG_PUTLN("chroma calibration");
    _setup_gtia_interface();
    uint16_t row = -1;

    calib_init();

    while (true)
    {
        UART_LOG_FLUSH();

        _wait_and_restart_dma(row);

        // swap buffers
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

/*



static void inline __scratch_y("_draw_chroma_row") _draw_chroma_row(uint16_t row)
{
    uint32_t y = row - FIRST_GTIA_ROW_TO_SHOW + SCREEN_OFFSET_Y;
    for (uint32_t i = 0; i < CHROMA_SAMPLES; i++)
    {
        int8_t matched = match_color(color_buf[buf_seq][i], pal_buf[buf_seq][i], row);
        plotf(i, y, gtia_palette[matched * 16 + 7]);
    }
}


#define MIN_COLOR 10
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

static __attribute__((noinline)) void __not_in_flash_func(_delays_calibration)(uint16_t row)
{
    static uint32_t x_counts[2];
    static uint32_t x_sums[2];
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
*/

#endif