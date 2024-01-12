#include "font8x8.h"
#include "gfx_text.h"
#include "gfx.h"


static inline void menu_show(){

    plot(1,1,RED);
    set_pos(4,4);
    set_fore_col(YELLOW);
    put_text("ATARI GTIA2DVI MENU");


    while (true)
    {
        tight_loop_contents();
    }
    



}