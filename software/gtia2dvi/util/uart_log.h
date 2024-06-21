#include <stdarg.h>
#include <stdio.h>
#include "pico/multicore.h"

#ifndef UART_LOG_H
#define UART_LOG_H

#define UART_LOG_BUF_SIZE 1024
#define UART_LOG_SERIAL_BAUD 230400

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
static spin_lock_t *uart_spin_lock;

char buf[256];

static inline void uart_log_init()
{
    uart_init(UART_ID, UART_LOG_SERIAL_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_spin_lock = spin_lock_instance(spin_lock_claim_unused(true));
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
    if (is_spin_locked(uart_spin_lock))
    {
        return;
    }

    int save = spin_lock_blocking(uart_spin_lock);
    while (uart_8n1_index_end != uart_8n1_index_cur)
    {
        if (!uart_is_writable(UART_ID))
            break;

        uart_putc(UART_ID, uart_8n1_buffer[uart_8n1_index_cur++]);

        if (uart_8n1_index_cur == UART_LOG_BUF_SIZE)
            uart_8n1_index_cur = 0;
    }
    spin_unlock(uart_spin_lock, save);
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
    int save = spin_lock_blocking(uart_spin_lock);
    while (*s)
    {
        uart_8n1_buffer[uart_8n1_index_end++] = *s++;
        if (uart_8n1_index_end == UART_LOG_BUF_SIZE)
        {
            uart_8n1_index_end = 0;
        }
    }
    spin_unlock(uart_spin_lock, save);
}
static __noinline void __not_in_flash_func(uart_log_putln)(char *s)
{
    uart_log_put(s);
    uart_log_put("\n");
}

static __noinline void __not_in_flash_func(uart_log_putlnf)(const char *format, ...)
{
    char buf[128]; 
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args); 
    va_end(args);

    uart_log_put(buf);
    uart_log_put("\n");
}

#endif