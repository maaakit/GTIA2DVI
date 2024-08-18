#ifndef GTIA_DUMP_H
#define GTIA_DUMP_H

#include "gtia_dump.pio.h"
#include "hardware/uart.h"

#define GTIA_DUMP_FORCE false
#define GTIA_DUMP_BUFFER_SIZE_BYTES 36000
#define GTIA_DUMP_BUFFER_PREVIOUS_LINE_OFFSET 17000
#define GTIA_DUMP_LINE_MIN 2
#define GTIA_DUMP_LINE_MAX 310

#define BIT(X, Y) (((X) >> (Y)) & 0x1)

#define UART_LOG_SERIAL_BAUD 230400 
#define UART_RX_PIN 9
#define UART_TX_PIN 8
#define UART_ID uart1


bool gtia_dump_is_requested()
{
    return btn_is_down(BTN_A) && btn_is_down(BTN_B);
}

uint8_t gtia_dump_data[GTIA_DUMP_BUFFER_SIZE_BYTES];

void gtia_dump_uart_init_custom()
{
    // Inicjalizacja UART
    uart_init(UART_ID, UART_LOG_SERIAL_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);
}

int gtia_dump_read_int_from_uart()
{
    char buffer[10];
    int idx = 0;

    while (true)
    {
        if (uart_is_readable(UART_ID))
        {
            char ch = uart_getc(UART_ID);
            if (ch >= '0' && ch <= '9')
            {
                buffer[idx++] = ch;
                uart_putc(UART_ID, ch);
            }
            else if (ch == '\r' || ch == '\n')
            {
                uart_putc(UART_ID, '\n');
                buffer[idx] = '\0';
                int number = atoi(buffer);
                if (number >= GTIA_DUMP_LINE_MIN && number <= GTIA_DUMP_LINE_MAX)
                {
                    return number;
                }
                else
                {
                    sprintf(buf, "; error: line number out of limit [%d - %d]\n; >", GTIA_DUMP_LINE_MIN, GTIA_DUMP_LINE_MAX);
                    uart_puts(UART_ID, buf);
                }
                idx = 0;
            }
        }
    }
}

void gtia_dump_process()
{

    vreg_set_voltage(VREG_VSEL);
    sleep_ms(100);

    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, false);

    gtia_dump_uart_init_custom();

    uart_puts(UART_ID, ";\n; please enter line number for dump and press [Enter]\n; >");

    uint requested_line = gtia_dump_read_int_from_uart();

    gpio_init_mask(INPUT_PINS_MASK);
    gpio_set_dir_in_masked(INPUT_PINS_MASK);

    dma_channel_config c = dma_channel_get_default_config(LUMA_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(GTIA_PIO, GTIA_LUMA_SM, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(LUMA_DMA_CHANNEL, &c, &gtia_dump_data, &GTIA_PIO->rxf[GTIA_LUMA_SM], (GTIA_DUMP_BUFFER_SIZE_BYTES) / 4, true);

    pio_sm_restart(GTIA_PIO, GTIA_LUMA_SM);
    uint offset_lm = pio_add_program(GTIA_PIO, &gtia_dump_program);
    gtia_dump_program_init(GTIA_PIO, GTIA_LUMA_SM, offset_lm, requested_line - 1);

    // wait for both DMA channels
    dma_channel_wait_for_finish_blocking(LUMA_DMA_CHANNEL);
    sprintf(buf, "; gtia dump row: %d\n", requested_line);
    uart_puts(UART_ID, buf);

    sprintf(buf, "; sampling rate: %d kHz\n", DVI_TIMING.bit_clk_khz);
    uart_puts(UART_ID, buf);

    uart_puts(UART_ID, ";===GTIA_DUMP_DATA_START===\n");

    uart_puts(UART_ID, "L0,L1,L2,L3,CSYNC,OSC,PAL,COLOR\n");

    for (int i = GTIA_DUMP_BUFFER_PREVIOUS_LINE_OFFSET; i < GTIA_DUMP_BUFFER_SIZE_BYTES; i++)
    {
        uint8_t val = gtia_dump_data[i];
        sprintf(buf, "%01d,%01d,%01d,%01d,%01d,%01d,%01d,%01d,%05d\n", BIT(val, 0), BIT(val, 1), BIT(val, 2), BIT(val, 3), BIT(val, 4), BIT(val, 5), BIT(val, 6), BIT(val, 7), i - GTIA_DUMP_BUFFER_PREVIOUS_LINE_OFFSET);
        //   sleep_ms(10);
        uart_puts(UART_ID, buf);
        //   uart_log_flush_blocking();
    }
    uart_puts(UART_ID, ";===GTIA_DUMP_DATA_STOP===\n");
    //  uart_log_flush_blocking();

    while (true)
    {
        sleep_ms(10000);
        uart_puts(UART_ID, ";\n");
        tight_loop_contents();
    }
}

#endif