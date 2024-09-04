#include "gtia_burst.pio.h"

#ifndef BURST_H
#define BURST_H

#define BURST_DMA_CHANNEL 6
#define BURST_SM 3
#define BURST_DATA_LENGTH 80

#define OSC_SIGNAL_VAL 0x2
#define COLOR_SIGNAL_VAL 0x8
#define BURST_FRAME_INTERVAL 499

uint32_t burst_data[BURST_DATA_LENGTH];
uint8_t burst_histo[256];

struct Fab
{
    uint16_t f;
    uint32_t a;
    uint32_t b;
};

struct Fab fabs[4];

uint offset_burst_prg;

static inline void burst_init()
{
    dma_channel_config c2 = dma_channel_get_default_config(BURST_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
    channel_config_set_dreq(&c2, pio_get_dreq(pio1, BURST_SM, false));
    channel_config_set_read_increment(&c2, false);
    channel_config_set_write_increment(&c2, true);
    channel_config_set_high_priority(&c2, true);

    dma_channel_configure(BURST_DMA_CHANNEL, &c2, &burst_data, &pio1->rxf[BURST_SM], BURST_DATA_LENGTH, false);

    pio_sm_restart(pio1, BURST_SM);
    offset_burst_prg = pio_add_program(pio1, &gtia_burst_program);
    gtia_burst_program_init(BURST_SM, offset_burst_prg);
}

static inline void _burst_process_histogram()
{
    int modx = 0;
    int pos = 2;
    int pos_out = 0;

    int histo_size = burst_histo[0];

    while (pos_out < histo_size)
    {
        int a = 0;
        int b = 0;

        while (burst_histo[pos] & OSC_SIGNAL_VAL)
            pos = pos + 2;

        int f = (burst_histo[pos] & COLOR_SIGNAL_VAL) > 0;
        if (f == 1)
        {
            while (burst_histo[pos] & COLOR_SIGNAL_VAL)
            {
                a += burst_histo[pos + 1];
                pos = pos + 2;
            }
            while (0 == (burst_histo[pos] & COLOR_SIGNAL_VAL))
            {
                b += burst_histo[pos + 1];
                pos = pos + 2;
            }
        }
        else
        {
            while (0 == (burst_histo[pos] & COLOR_SIGNAL_VAL))
            {
                a += burst_histo[pos + 1];
                pos = pos + 2;
            }
            while ((burst_histo[pos] & COLOR_SIGNAL_VAL))
            {
                b += burst_histo[pos + 1];
                pos = pos + 2;
            }
        }
        burst_histo[pos_out++] = f;
        burst_histo[pos_out++] = a;
        burst_histo[pos_out++] = b;

        struct Fab tmp = fabs[modx & 3];
        tmp.f += f;
        tmp.a += a;
        tmp.b += b;

        fabs[modx & 3] = tmp;

        modx++;
        if (modx == 4)
            return;

        while (!(burst_histo[pos] & OSC_SIGNAL_VAL))
            pos = pos + 2;
    }
}

static inline void _burst_process_sample_data()
{
    int last = -1;
    int cnt = 0;
    int histo_i = 0;
    for (int i = 0; i < BURST_DATA_LENGTH; i++)
    {
        int dat = burst_data[i] & 0xaaaaaaaa;

        for (int j = 0; j < 8; j++)
        {
            int val = dat & 0xf;
            dat = dat >> 4;

            if (val == last)
                cnt++;
            else
            {
                burst_histo[histo_i++] = last;
                burst_histo[histo_i++] = cnt;
                last = val;
                cnt = 1;
            }
        }
    }
    burst_histo[0] = histo_i;
}

static inline void burst_analyze(int row)
{
    if (row == 309)
    {
        dma_channel_wait_for_finish_blocking(BURST_DMA_CHANNEL);
        _burst_process_sample_data();
    }
    if (row == 310)
    {
        if ((frame_count % (BURST_FRAME_INTERVAL + 1)) == 0)
        {
            uart_log_putlnf("---- frame: %d", frame_count);
            for (uint i = 0; i < 4; i++)
            {
                struct Fab f = fabs[i];
                uart_log_putlnf("%d    %01d %6.2f %6.2f", i, (f.f + (BURST_FRAME_INTERVAL / 2)) / BURST_FRAME_INTERVAL, ((float)f.a / BURST_FRAME_INTERVAL), ((float)f.b / BURST_FRAME_INTERVAL));
            }
            memset(fabs, 0, sizeof(fabs));
        }
        else
        {
            _burst_process_histogram();
        }
    }
}

#endif