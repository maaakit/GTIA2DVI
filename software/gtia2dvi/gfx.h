// #include <stdio.h>
// #include <stdlib.h>
#include "cfg.h"
#include <stdint.h>
#include "pico/platform.h"

#ifndef GFX_H
#define GFX_H

#define BLACK (0)
#define RED (0b11111 << 11)
#define GREEN (0b111111 << 5)
#define BLUE (0b11111)
#define YELLOW (RED + GREEN)
#define WHITE (RED + GREEN + BLUE)
#define MAGENTA (RED + BLUE)
#define CYAN (GREEN + BLUE)
#define GRAY1 ((7 << 11) + (7 << 5) + 7)
#define GRAY2 ((14 << 11) + (14 << 5) + 14)

struct gfx
{
    uint16_t posx;
    uint8_t posy;
    int16_t fore_col;
    int16_t bkg_col;
    uint16_t frame_cnt;
};

struct gfx gfx_state = {0, 0, 0, 0, 0};

static void __not_in_flash("set_pos") set_pos(uint16_t x, uint16_t y)
{
    gfx_state.posx = x;
    gfx_state.posy = y;
}

uint16_t framebuf[FRAME_WIDTH * FRAME_HEIGHT];

static void __not_in_flash("set_fore_col") set_fore_col(int16_t color_rgb_565)
{
    gfx_state.fore_col = color_rgb_565;
}
static void __not_in_flash("set_back_col") set_back_col(int16_t color_rgb_565)
{
    gfx_state.bkg_col = color_rgb_565;
}
static inline void __not_in_flash("plot") plot(uint16_t x, uint16_t y, int16_t color_rgb_565)
{
    if (x<FRAME_WIDTH && y<FRAME_HEIGHT && x>0 && y>0)
    framebuf[y * FRAME_WIDTH + x] = color_rgb_565;
}

#define RGB565(R, G, B) ((((R) >> 3) & 0x1f) << 11) | ((((G) >> 2) & 0x3f) << 5) | (((B) >> 3) & 0x1f)
#define LUM(X) RGB565(X, X, X)
static const uint16_t colors[] = {
	LUM(0), LUM(16), LUM(16 * 2), LUM(16 * 3), LUM(16 * 4), LUM(16 * 5),
	LUM(16 * 6), LUM(16 * 7), LUM(16 * 8), LUM(16 * 9), LUM(16 * 10), LUM(16 * 11),
	LUM(16 * 12), LUM(16 * 13), LUM(16 * 14), LUM(16 * 15), LUM(255),
	BLACK, RED, BLUE, WHITE, GREEN, YELLOW, MAGENTA, CYAN};

static uint16_t *ptr_framebuf = framebuf;

static inline void __not_in_flash("plotf") plotf(uint32_t x, uint32_t y, int16_t color_rgb_565)
{
	if (x < FRAME_WIDTH && y < FRAME_HEIGHT)
		*(ptr_framebuf + (y * FRAME_WIDTH + x)) = color_rgb_565;
}
#endif // GFX_H