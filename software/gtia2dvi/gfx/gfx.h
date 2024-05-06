#include "../cfg.h"
#include <stdint.h>
#include "pico/platform.h"

#ifndef GFX_H
#define GFX_H

struct gfx
{
    uint16_t posx;
    uint8_t posy;
    int8_t fore_col;
    int8_t bkg_col;
    uint16_t frame_cnt;
};

struct gfx gfx_state = {0, 0, 0, 0, 0};

static void __not_in_flash("set_pos") set_pos(uint16_t x, uint16_t y)
{
    gfx_state.posx = x;
    gfx_state.posy = y;
}

uint8_t framebuf[FRAME_WIDTH * FRAME_HEIGHT];

static void inline fill_scren(int8_t color)
{
    uint16_t *buf = (uint32_t *)framebuf;
    uint16_t count = FRAME_WIDTH * FRAME_HEIGHT / 2;
    uint16_t val = (color << 16) + color;
    while (count--)
        *buf++ = val;
}

static void __not_in_flash("set_fore_col") set_fore_col(int8_t color)
{
    gfx_state.fore_col = color;
}
static void __not_in_flash("set_back_col") set_back_col(int8_t color)
{
    gfx_state.bkg_col = color;
}
static inline void __not_in_flash("plot") plot(uint16_t x, uint16_t y, int8_t color)
{
    if (x < FRAME_WIDTH && y < FRAME_HEIGHT && x > 0 && y > 0)
        framebuf[y * FRAME_WIDTH + x] = color;
}


static uint8_t *ptr_framebuf = framebuf;

static inline void box(uint x, uint y, uint w, uint h, uint color)
{
    for (int i = x; i < x + w; i++)
    {
        plot(i, y, color);
        plot(i, y + h, color);
    }
    for (int i = y; i < y + h; i++)
    {
        plot(x, i, color);
        plot(x + w, i, color);
    }
}

static inline void __not_in_flash("plotf") plotf(uint32_t x, uint32_t y, int8_t color)
{
    if (x < FRAME_WIDTH && y < FRAME_HEIGHT)
        *(ptr_framebuf + (y * FRAME_WIDTH + x)) = color;
}
#endif // GFX_H