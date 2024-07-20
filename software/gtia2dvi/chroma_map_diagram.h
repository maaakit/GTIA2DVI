#ifndef CHROMA_MAP_DIAGRAM_H
#define CHROMA_MAP_DIAGRAM_H

#include "chroma_calibration.h"

#define MAP_DIAGRAM_BOX_SIZE 36
#define MAP_DIAGRAM_OFFSET_X 232
#define MAP_DIAGRAM_OFFSET_Y 16

#define MAP_DIAGRAM_BOX_SIZE_BIG 68
#define MAP_DIAGRAM_OFFSET_X_BIG 60
#define MAP_DIAGRAM_OFFSET_Y_BIG 10

#define NO_MAPPING_PIXEL_COLOR 2

static inline void plot_diag_pixel(int16_t dec, uint i, uint row, int col)
{
    int f = DEC_F(dec);
    int a = DEC_A(dec);
    int b = DEC_B(dec);

    plotf(MAP_DIAGRAM_OFFSET_X + (i & 0x3) * MAP_DIAGRAM_BOX_SIZE + a, MAP_DIAGRAM_OFFSET_Y + (row % 2) * MAP_DIAGRAM_BOX_SIZE + (f * MAP_DIAGRAM_BOX_SIZE * 2) + b, col);
}

static inline void _plot_diag_pixel_big(int16_t dec, uint i, uint row, int col)
{
    int f = DEC_F(dec);
    int a = DEC_A(dec);
    int b = DEC_B(dec);

    int pos_x = ((i & 0x3) * MAP_DIAGRAM_BOX_SIZE_BIG + (a * 2));
    int pos_y = ((row % 2) * MAP_DIAGRAM_BOX_SIZE_BIG + (f * MAP_DIAGRAM_BOX_SIZE_BIG * 2) + (b * 2));

    int pxl_color = (col > 0) ? col * 16 + 6 : NO_MAPPING_PIXEL_COLOR;

    plotf(MAP_DIAGRAM_OFFSET_X_BIG + pos_x, MAP_DIAGRAM_OFFSET_Y_BIG + pos_y, pxl_color);

    if (col > 0)
    {
        plotf(MAP_DIAGRAM_OFFSET_X_BIG + pos_x + 1, MAP_DIAGRAM_OFFSET_Y_BIG + pos_y, pxl_color);
        plotf(MAP_DIAGRAM_OFFSET_X_BIG + pos_x, MAP_DIAGRAM_OFFSET_Y_BIG + pos_y + 1, pxl_color);
        plotf(MAP_DIAGRAM_OFFSET_X_BIG + pos_x + 1, MAP_DIAGRAM_OFFSET_Y_BIG + pos_y + 1, pxl_color);
        uart_log_putlnf("%03X %01d.%02d.%02d r:%d x:%d c:%d", dec, f, a, b, row % 2, i & 0x3, col);
        uart_log_flush_blocking();
    }
}

static inline void update_mapping_diagram_err(int16_t dec, uint i, uint row)
{
    plot_diag_pixel(dec, i, row, WHITE);
}

static inline int _read_calibrated_color(int row, int modx, int decode)
{
    return calibration_data[row][modx][decode];
}

static inline void update_mapping_diagram(int16_t decode)
{
    for (int modx = 0; modx < 4; modx++)
        for (int row = 0; row < 2; row++)
        {
            int color = _read_calibrated_color(row, modx, decode);
            plot_diag_pixel(decode, modx, row, color);
        }
}

static inline void dump_mapping_diagram(int16_t decode)
{
    for (int modx = 0; modx < 4; modx++)
        for (int row = 0; row < 2; row++)
        {
            int color = _read_calibrated_color(row, modx, decode);
            _plot_diag_pixel_big(decode, modx, row, color);
        }
}

#endif