
#ifndef UART_LOG_H
#define UART_LOG_H

#define UART_LOG_BUF_SIZE 128
#define UART_LOG_SERIAL_BAUD 1000000

#define UART_RX_PIN 9
#define UART_TX_PIN 8
#define UART_ID uart1

#ifdef LOG_ENABLED
#define UART_LOG_PUTLN(msg) uart_log_putln(msg)
#define UART_LOG_FLUSH() uart_log_flush()
#else
#define UART_LOG_PUTLN(msg) ((void)0)
#define UART_LOG_FLUSH(msg) ((void)0)
#endif

static char uart_8n1_buffer[UART_LOG_BUF_SIZE];

static uint16_t uart_8n1_index_end = 0;
static uint16_t uart_8n1_index_cur = 0;

char buf[128];

static inline void uart_log_init()
{
    uart_init(UART_ID, UART_LOG_SERIAL_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

static inline void uart_update_clkdiv()
{
    uart_init(UART_ID, UART_LOG_SERIAL_BAUD);
}

static inline char __not_in_flash_func(get_char)()
{
    if (uart_is_readable(UART_ID))
    {
        return ((uart_hw_t *)UART_ID)->dr;
    }
    return 0;
}

static inline void __not_in_flash_func(uart_log_flush)()
{

    while (uart_8n1_index_end != uart_8n1_index_cur)
    {
        if (!uart_is_writable(UART_ID))
            return;

        uart_putc(UART_ID, uart_8n1_buffer[uart_8n1_index_cur++]);

        if (uart_8n1_index_cur == UART_LOG_BUF_SIZE)
            uart_8n1_index_cur = 0;
    }
}

static inline void __not_in_flash_func(uart_log_flush_blocking)()
{
    while (uart_8n1_index_end != uart_8n1_index_cur)
    {
        uart_log_flush();
    }
}

static inline void __not_in_flash_func(uart_log_put)(char *s)
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
static __noinline void __not_in_flash_func(uart_log_putln)(char *s)
{
    uart_log_put(s);
    uart_log_put("\n");
}

#endif