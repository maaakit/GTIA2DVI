#ifndef COLOR_BURST_H
#define COLOR_BURST_H

#define CB_STAT_SIZE 32

uint32_t cb_stats[2][CB_STAT_SIZE * 2];

void static inline _cb_add_signature(uint32_t signature, uint row)
{
    if (signature == 0)
    {
        return;
    }
    for (int i = 0; i < CB_STAT_SIZE * 2; i += 2)
    {
        if (cb_stats[row % 2][i] == signature) // found that value
        {
            cb_stats[row % 2][i + 1]++;
            return;
        }
        if (cb_stats[row % 2][i] == 0) // empty place
        {
            cb_stats[row % 2][i] = signature;
            cb_stats[row % 2][i + 1] = 1;
            return;
        }
    }
}

void static inline _cb_dump_data(int row)
{
    sprintf(buf, "frame: %d %d", frame, row % 2);
    uart_log_putln(buf);

    int odd_parity = row % 2;

    int index_of_max = 0;

    for (int i = 2; i < CB_STAT_SIZE * 2; i += 2)
    {
        if (cb_stats[odd_parity][i + 1] > cb_stats[odd_parity][index_of_max + 1])
        {
            index_of_max = i;
        }
    }

    uint total = 0;

    for (int i = 0; i < CB_STAT_SIZE * 2; i += 2)
    {
        // compute total
        total += cb_stats[odd_parity][i + 1];
    }

    sprintf(buf, "%08X * %d of total %d", cb_stats[odd_parity][index_of_max], cb_stats[odd_parity][index_of_max + 1], total);
    uart_log_putln(buf);


    for (int i = 0; i < CB_STAT_SIZE * 2; i += 2)
    {
        // reset values
        cb_stats[odd_parity][i] = 0;
        cb_stats[odd_parity][i + 1] = 0;
    }
}

void static __noinline __not_in_flash_func(color_burst_process)(int row)
{

    if (frame % 500 == 0)
    {
        _cb_dump_data(row);
    }
    else
    {
        for (int x = 1; x < 12; x++)
        {
            uint32_t color = color_buf[buf_seq][x];
            uint32_t pal = pal_buf[buf_seq][x];
            uint32_t signature = color ^ pal;
            _cb_add_signature(signature, row);
        }
    }
}

#endif