#include "gtia_luma_ng.pio.h"
#include "gtia_pal.pio.h"
#include "gtia_chroma_v2.pio.h"
#include "gtia_palette.h"
#include "util/buttons.h"
#include "util/uart_log.h"

#include "chroma_calib.h"

#ifndef VIDEO_CHANNEL_H
#define VIDEO_CHANNEL_H

#define GTIA_LUMA_SM 0
#define GTIA_PAL_SM 1
#define GTIA_COLOR_SM 2
#define GTIA_PIO pio1

#define LUMA_DMA_CHANNEL 9
#define COLOR_DMA_CHANNEL 10
#define PAL_DMA_CHANNEL 11

#define LUMA_LINE_LENGTH_BYTES 202

#define LUMA_START_OFFSET 12

#define INPUT_PINS_MASK 0xff

static inline void calibrate_luma();

void __attribute__((noinline)) __not_in_flash_func(calibrate_chroma)();

static inline void __scratch_y("wait_and_restart_dma") _wait_and_restart_dma();

uint16_t luma_buf[LUMA_LINE_LENGTH_BYTES / 2];

static inline void _wait_and_restart_dma()
{
#ifdef LED_WAIT_DMA
    gpio_put(LED_PIN, 1);
#endif
    // wait for both DMA channels

    dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);
    // dma_channel_wait_for_finish_blocking(PAL_DMA_CHANNEL);
    // dma_channel_wait_for_finish_blocking(COLOR_DMA_CHANNEL);

    // restart DMA channels
    dma_channel_set_write_addr(PAL_DMA_CHANNEL, &pal_buf[buf_seq], true);
    dma_channel_set_write_addr(COLOR_DMA_CHANNEL, &color_buf[buf_seq], true);
    dma_channel_set_write_addr(LUMA_DMA_CHANNEL, &luma_buf, true);
#ifdef LED_WAIT_DMA
    gpio_put(LED_PIN, 0);
#endif
}

static inline void __not_in_flash_func(_setup_gtia_interface)()
{
    gpio_init_mask(INPUT_PINS_MASK);
    gpio_set_dir_in_masked(INPUT_PINS_MASK);

    dma_channel_config c = dma_channel_get_default_config(LUMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(GTIA_PIO, GTIA_LUMA_SM, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(LUMA_DMA_CHANNEL, &c, &luma_buf, &GTIA_PIO->rxf[GTIA_LUMA_SM], (LUMA_LINE_LENGTH_BYTES + 2) / 4, false);

    dma_channel_config c2 = dma_channel_get_default_config(PAL_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
    channel_config_set_dreq(&c2, pio_get_dreq(GTIA_PIO, GTIA_PAL_SM, false));
    channel_config_set_read_increment(&c2, false);
    channel_config_set_write_increment(&c2, true);
    channel_config_set_high_priority(&c2, true);
    dma_channel_configure(PAL_DMA_CHANNEL, &c2, &pal_buf, &GTIA_PIO->rxf[GTIA_PAL_SM], CHROMA_SAMPLES, false);

    dma_channel_config c3 = dma_channel_get_default_config(COLOR_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c3, DMA_SIZE_32);
    channel_config_set_dreq(&c3, pio_get_dreq(GTIA_PIO, GTIA_COLOR_SM, false));
    channel_config_set_read_increment(&c3, false);
    channel_config_set_write_increment(&c3, true);
    channel_config_set_high_priority(&c3, true);
    dma_channel_configure(COLOR_DMA_CHANNEL, &c3, &color_buf, &GTIA_PIO->rxf[GTIA_COLOR_SM], CHROMA_SAMPLES, false);

    uint offset_lm = pio_add_program(GTIA_PIO, &gtia_luma_ng_program);
    uint offset_chv2 = pio_add_program(GTIA_PIO, &gtia_chroma_v2_program);
    uint offset_pal = pio_add_program(GTIA_PIO, &gtia_pal_v2_program);

    gtia_pal_v2_program_init(GTIA_PAL_SM, offset_pal, CHROMA_SAMPLES, PAL_PIN);
    gtia_chroma_v2_program_init(GTIA_COLOR_SM, offset_chv2, CHROMA_SAMPLES, COLOR_PIN);
    gtia_luma_ng_program_init(GTIA_PIO, GTIA_LUMA_SM, offset_lm, LUMA_LINE_LENGTH_BYTES);
}

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
    for (uint32_t i = LUMA_START_OFFSET; i < (LUMA_LINE_LENGTH_BYTES / 2)-2; i++)
    {
        uint32_t c = luma_buf[i];
        uint32_t luma = c & 0xf;
        uint16_t col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *pixel_ptr++ = col565;

        luma = (c >> 4) & 0xf;
        color_ptr++;
        pal_ptr++;

        matched = match_color(*color_ptr, *pal_ptr, row);
        // if (matched == -1)
        // {
        //     matched = match_color(*(pixel_ptr + 1), row);
        // }

        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *pixel_ptr++ = col565;

        luma = (c >> 8) & 0xf;
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *pixel_ptr++ = col565;

        luma = (c >> 12) & 0xf;
        color_ptr += ((i) & 0x1) + 1;
        pal_ptr += ((i) & 0x1) + 1;
        // color_ptr++;
        // pal_ptr++;

        matched = match_color(*color_ptr, *pal_ptr, row);
        // if (matched == -1)
        // {
        //     matched = match_color(*(chroma_sample_ptr + 1), row);
        // }
        col565 = matched != -1 ? gtia_palette[matched * 16 + luma] : INVALID_CHROMA_HANDLER;
        *pixel_ptr++ = col565;
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

static uint white_col_index = 0;
static uint dump_row_index = 60;
static uint mode = 2;
static bool force_dump = false;

void static handleButtons()
{
    char buf[32];
    enum BtnEvent btn = btn_last_event();
    if (btn == BTN_A_SHORT && preset_loaded)
    {
        mode = (mode + 1) % 5;
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
        plotf(8, dump_row_index, BLACK);

        //   UART_LOG_PUTLN("BTN_B");
        white_col_index = (white_col_index + 1);
        dump_row_index = (dump_row_index + 1) % 288;

        sprintf(buf, "c %04x", white_col_index);
        plotf(8, dump_row_index, WHITE);
        UART_LOG_PUTLN(buf);
    }
}

void __not_in_flash_func(process_video_stream)()
{
    // chroma_hwd_init(true);

    _setup_gtia_interface();

    uint16_t row;
    while (true)
    {
        enum BtnEvent btn = btn_last_event();

        if (btn == BTN_A_SHORT && preset_loaded)
        {
            app_cfg.enableChroma = !app_cfg.enableChroma;
            UART_LOG_PUTLN("BTN_A");
        }
        if (btn == BTN_B_SHORT)
        {
            UART_LOG_PUTLN("BTN_B");
        }
        UART_LOG_FLUSH();
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

// //#define CALIB_CHROMA x
// void __scratch_y("process_video_stream") process_video_stream()
// {
//     _setup_gtia_interface();
//     uint burst;
//     uint16_t row = -1;
//     char buf[80];

// #ifdef CALIB_CHROMA
//     calib_init();
// #endif

//     while (true)
//     {

//         if (row == 4)
//         {
//             handleButtons();
//         }
//         UART_LOG_FLUSH();

//         _wait_and_restart_dma();

//         // swap buffer
//         buf_seq = (buf_seq + 1) % 2;

//         // pio returns line as negative number so change sign
//         row = -luma_buf[0];

//         if (row == 4)
//         {
//             // increase frame counter once per frame
//             frame++;
//         }
//         if (row == 8 || row == 9)
//         {
//             // if ((frame % 1000) == 0)
//             // {
//             //     uart_log_put("BRST:");
//             //     for (int i = 1; i < 11; i++)
//             //     {
//             //         sprintf(buf, " %04x,", decode_color(i, buf_seq));
//             //         uart_log_put(buf);
//             //     }
//             //     uart_log_putln("");
//             // }
//         }

//         if (row < FIRST_GTIA_ROW_TO_SHOW || row >= FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW)
//         {
// #ifdef CALIB_CHROMA
//             if (calib_finished == false)
//                 calib_handle(row);
// #endif
//             // skip rows outside view port
//             continue;
//         }
// #ifdef CALIB_CHROMA
//         if (calib_finished)
//         {
//             plotf(8, 8, GREEN);

//             if (row > 50 && row <= 65)
//             {
//                 for (int c = 0; c < 15; c++)
//                 {
//                     for (int i = 0; i < SAMPLE_X_SIZE; i++)
//                         plotf(SAMPLE_X_FIRST + ((c * 25) / 2) + i, row - FIRST_GTIA_ROW_TO_SHOW, gtia_palette[(c + 1) * 16 + 8]);
//                 }
//             }
//             else

//                 for (uint x = 20; x < CHROMA_SAMPLES; x++)
//                 {
//                     uint16_t sample = decode_color(x, buf_seq);
//                     uint8_t color = chroma_table[sample][row % 2];
//                     plotf(x, row - FIRST_GTIA_ROW_TO_SHOW, color == -1 ? MAGENTA : gtia_palette[color * 16 + 8]);
//                 }
//         }
//         else
//         {
//             plotf(8, 8, YELLOW);
//             calib_redraw(row, buf_seq);
//         }
// #else

//         if (mode == 0 || mode == 1)
//         {
//             _draw_luma_only_row(row);
//         }
//         else if ((mode == 2) || ((mode == 3) && (row % 2)) || ((mode == 4) && ((row % 2) == 0)))
//         {

//             // if (row==120)
//             // __breakpoint();
//             // burst = decode_color(1, buf_seq, 0);
//             for (uint x = 20; x < CHROMA_SAMPLES; x++)
//             {
//                 uint matched = decode_color(x, buf_seq);

//                 plotf(x + 10, row - FIRST_GTIA_ROW_TO_SHOW, matched == white_col_index ? MAGENTA : matched); // color_buf[buf_seq][x]);
//             }
//         }
//         else
//         {
//             for (uint x = 0; x < CHROMA_SAMPLES - 0; x++)
//             {
//                 plotf(x + 10, row - FIRST_GTIA_ROW_TO_SHOW, BLACK); // color_buf[buf_seq][x]);
//             }
//         }
//         plotf(8, 8, MAGENTA);
// #endif
//     }
// }

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

void __attribute__((noinline)) __scratch_y("calibrate_chroma") calibrate_chroma()
{
    UART_LOG_PUTLN("chromas calibration");
    _setup_gtia_interface();
    //   uint burst;
    uint16_t row = -1;
    //  char buf[80];

    calib_init();

    while (true)
    {

        // if (row == 4)
        // {
        //     handleButtons();
        // }
        UART_LOG_FLUSH();

        _wait_and_restart_dma();

        // swap buffer
        buf_seq = (buf_seq + 1) % 2;

        // pio returns line as negative number so change sign
        row = -luma_buf[0];

        if (row == 4)
        {
            // increase frame counter once per frame
            frame++;
        }
        // if (row == 8 || row == 9)
        // {
        //     // if ((frame % 1000) == 0)
        //     // {
        //     //     uart_log_put("BRST:");
        //     //     for (int i = 1; i < 11; i++)
        //     //     {
        //     //         sprintf(buf, " %04x,", decode_color(i, buf_seq));
        //     //         uart_log_put(buf);
        //     //     }
        //     //     uart_log_putln("");
        //     // }
        // }

        if (row < FIRST_GTIA_ROW_TO_SHOW || row >= FIRST_GTIA_ROW_TO_SHOW + GTIA_ROWS_TO_SHOW)
        {
            // if (calib_finished == false)
            calib_handle(row);
        }
        else
        {
            calib_redraw(row, buf_seq);
        }

        // if (calib_finished)
        // {
        //     plotf(8, 8, GREEN);

        //     // if (row > 50 && row <= 65)
        //     // {
        //     //     for (int c = 0; c < 15; c++)
        //     //     {
        //     //         for (int i = 0; i < SAMPLE_X_SIZE; i++)
        //     //             plotf(SAMPLE_X_FIRST + ((c * 25) / 2) + i, row - FIRST_GTIA_ROW_TO_SHOW, gtia_palette[(c + 1) * 16 + 8]);
        //     //     }
        //     // }
        //     // else
        //     if (row == 200)
        //     {
        //         // UART_LOG_PUTLN("saving preset");
        //         // app_cfg.enableChroma = true;
        //         // set_post_boot_action(WRITE_CONFIG);
        //         // set_post_boot_action(WRITE_PRESET);
        //         // watchdog_enable(1, 1);
        //         // while (true)
        //         // {
        //         //     UART_LOG_FLUSH();
        //         // }
        //     }
        //     else
        //     {

        //         for (uint x = 20; x < CHROMA_SAMPLES; x++)
        //         {
        //             uint16_t sample = decode_color(x, buf_seq);
        //             uint8_t color = chroma_table[sample][row % 2];
        //             plotf(x, row - FIRST_GTIA_ROW_TO_SHOW, color == -1 ? MAGENTA : gtia_palette[color * 16 + 8]);
        //         }
        //     }
        // }
        // else
        // {
        //  plotf(8, 8, YELLOW);

        // }
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