#include "uart_8n1_tx.pio.h"

#ifndef UART_LOG_H
#define UART_LOG_H

#define UART_LOG_BUF_SIZE 1024
#define UART_LOG_PIO pio0
#define UART_LOG_SM 3
#define UART_LOG_SERIAL_BAUD 2000000
#define UART_LOG_TX_PIN 22

static char uart_8n1_buffer[UART_LOG_BUF_SIZE];

static uint16_t uart_8n1_index_end = 0;

static uint16_t uart_8n1_index_cur = 0;

static inline void uart_log_init()
{
    uart_8n1_tx_program_init(UART_LOG_PIO, UART_LOG_SM, pio_add_program(UART_LOG_PIO, &uart_8n1_tx_program), UART_LOG_TX_PIN, UART_LOG_SERIAL_BAUD);
}

static inline void uart_update_clkdiv()
{
    uart_8n1_tx_update_clkdiv(UART_LOG_PIO, UART_LOG_SM, UART_LOG_SERIAL_BAUD);
}

// TODO: non blocking DMA here !
static inline void uart_log_flush()
{
    while (uart_8n1_index_end != uart_8n1_index_cur)
    {
        if (pio_sm_is_tx_fifo_full(UART_LOG_PIO, UART_LOG_SM))
        {
            return;
        }
        pio_sm_put(UART_LOG_PIO, UART_LOG_SM, uart_8n1_buffer[uart_8n1_index_cur++]);

        if (uart_8n1_index_cur == UART_LOG_BUF_SIZE)
        {
            uart_8n1_index_cur = 0;
        }
    }
}

static inline void uart_log_flush_blocking()
{
    while (uart_8n1_index_end != uart_8n1_index_cur)
    {
        uart_log_flush();
    }
}

static inline void uart_log_put(char *s)
{
    while (*s)
    {
        uart_8n1_buffer[uart_8n1_index_end++] = *s++;
        if (uart_8n1_index_end == UART_LOG_BUF_SIZE)
        {
            uart_8n1_index_end = 0;
        }
    }
}
static inline void uart_log_putln(char *s)
{
    uart_log_put(s);
    uart_log_put("\n");
}
static inline void uart_log_transfer(uint8_t *data, uint size)
{
    // to be implemented with DMA
}
static inline void uart_log_transfer16(uint16_t *data, uint size)
{
    // to be implemented with DMA
}
static inline void uart_log_transfer32(uint32_t *data, uint size)
{
    // to be implemented with DMA
}

#endif