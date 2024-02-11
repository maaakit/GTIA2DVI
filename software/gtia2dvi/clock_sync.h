

#ifndef CLOCK_SYNC_H
#define CLOCK_SYNC_H

void setup_atari_clocks()
{
    gpio_set_function(20, GPIO_FUNC_GPCK);
    gpio_set_function(22, GPIO_FUNC_GPCK);
}

void measure_freq(int clock, char *name)
{
    char buf[32];
    uint freq = frequency_count_khz(clock);
    sprintf(buf, "%s freq: %dkHz", name, freq);
    uart_log_putln(buf);
}

#endif