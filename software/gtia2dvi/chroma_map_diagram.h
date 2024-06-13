#ifndef CHROMA_MAP_DIAGRAM_H
#define CHROMA_MAP_DIAGRAM_H

#include "chroma_calibration.h"

#define MAP_DIAGRAM_BOX_SIZE 36
#define MAP_DIAGRAM_OFFSET_X 232
#define MAP_DIAGRAM_OFFSET_Y 16

static inline void update_mapping_diagram_c(int16_t dec, uint i, uint row, int col)
{
    int f = dec & 0x1;
    int a = (dec >> 1) & 0x1f;
    int b = (dec >> 6) & 0x1f;

    plotf(MAP_DIAGRAM_OFFSET_X + (i % 4) * MAP_DIAGRAM_BOX_SIZE + a, MAP_DIAGRAM_OFFSET_Y + (row % 2) * MAP_DIAGRAM_BOX_SIZE + (f * MAP_DIAGRAM_BOX_SIZE * 2) + b, col);
}

static inline void update_mapping_diagram_err(int16_t dec, uint i, uint row)
{
    update_mapping_diagram_c(dec, i, row, WHITE);
}

static inline void update_mapping_diagram(int16_t decode)
{
    int color_index, color;

    for (int mod = 0; mod < 4; mod++)
        for (int row = 0; row < 2; row++)
        {
            color_index = calibration_data[row][mod][decode];
            color = (color_index > 0) ? color_index * 16 + 6 : 2;
            update_mapping_diagram_c(decode, mod, row, color);
        }
}

#endif