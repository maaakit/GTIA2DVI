
#include <stdio.h>
#include <stdlib.h>
#include "gfx.h"

static void __not_in_flash("put_char") put_char(char ascii)
{
    if (ascii<FONT_FIRST_CHAR || ascii> (FONT_FIRST_CHAR+FONT_CHARS_NUMBER )){
        return;
    }
    for (uint8_t xx = 0; xx < FONT_WIDTH; xx++)
    {
        uint8_t data = font_data[(ascii - FONT_FIRST_CHAR) * FONT_WIDTH + xx];
        for (uint8_t yy = 0; yy < FONT_HEIGHT; yy++)
        {

            if (data & 0x01)
            {
                plot(gfx_state.posx + xx, gfx_state.posy + yy, gfx_state.fore_col);
            }
            else
            {
                plot(gfx_state.posx + xx, gfx_state.posy + yy, gfx_state.bkg_col);
            }
            data = (data >> 1);
        }
    }
    gfx_state.posx = gfx_state.posx + FONT_WIDTH;
}


static void __not_in_flash("put_text") put_text(char *str)
{

    while (*str != '\0')
    {
        put_char(*str);
        str++;
    }
}

struct text_block
{
    uint16_t posx;
    uint8_t posy;
    uint8_t c_width;
    uint8_t c_height;

    uint8_t c_pos_x;
    uint8_t c_pos_y;
    int16_t fore_col;
    int16_t bkg_col;
};

struct text_block tb = {};

static void __not_in_flash("text_block_init") text_block_init(uint16_t posx, uint8_t posy, uint8_t c_width, uint8_t c_height)
{
    tb.posx = posx;
    tb.posy = posy;
    tb.c_width = c_width;
    tb.c_height = c_height;
    tb.c_pos_x = 0;
    tb.c_pos_y = 0;
}

static void __not_in_flash("scroll_up") scroll_vert(int16_t rows)
{
    //  for( uint_8 y = )
}

static void __not_in_flash("check_space") check_space()
{
    if (tb.c_pos_x >= tb.c_width)
    {
        tb.c_pos_x = 0;
        tb.c_pos_y++;
    }
    if (tb.c_pos_y >= tb.c_height)
    {
        tb.c_pos_y = tb.c_height - 1;
        // scroll here!
        scroll_vert(-8);
    }
}

static void __not_in_flash("text_block_print") text_block_print(char *str)
{
    check_space();

    set_pos(tb.posx + tb.c_pos_x * FONT_WIDTH, tb.posy + tb.c_pos_y * FONT_HEIGHT);
    while (*str != '\0')
    {
        put_char(*str);
        tb.c_pos_x++;
        check_space();
        str++;
    }
}

static void __not_in_flash("text_block_print_chr") text_block_print_chr(char c)
{
    if (c == '\r')
    {
        tb.c_pos_x = 0;
    }
    else if (c == '\n')
    {
        tb.c_pos_y++;
    }
    else
    {
        set_pos(tb.posx + tb.c_pos_x * FONT_WIDTH, tb.posy + tb.c_pos_y * FONT_HEIGHT);
        put_char(c);
        tb.c_pos_x++;
    }
    check_space();
}

static void __not_in_flash("text_block_println") text_block_println(char *str)
{
    text_block_println(str);
    text_block_println("\r\n");
}
static void __not_in_flash("text_block_scroll") text_block_scroll(char *str)
{
}
static void __not_in_flash("text_block_set_cur_pos") text_block_set_cur_pos(char *str)
{
}
