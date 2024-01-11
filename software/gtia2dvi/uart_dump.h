#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"


#define UART_ID uart0
// Baud rate used for communication
#define BAUD_RATE 115200

void dump_data( char *data, int size  ) {
	    uart_init(UART_ID, BAUD_RATE);
    // Set up GPIO pins for UART transmit
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    // Set up UART parameters
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, false);

    // Send data
    uart_write_blocking(UART_ID, data, size);
}