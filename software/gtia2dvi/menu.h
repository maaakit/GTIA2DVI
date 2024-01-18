#include "gfx_text.h"
#include "gfx.h"

#define CHOOSEN_OPTION_COLOR GREEN
#define ACTIVE_OPTION_COLOR WHITE
#define DISABLED_OPTION_COLOR CYAN
#define MENU_ITEM_POS_Y(i)  ((i) * 10 + 30)

struct MenuItem
{
    char *optionName;
    void (*functionPointer)();
};

struct Menu
{
    const char *header;
    const char *menuTitle;
    struct MenuItem *items;
    int itemCount;
    int currentItem;
};



static inline void updateMenu(struct Menu *menu)
{
    enum btn_event btn = btn_last_event();

    if (btn == BTN_A_SHORT)
    {
        menu->currentItem++;
        plot(1,menu->currentItem+10,GREEN);
        if (menu->currentItem >= menu->itemCount)
        {
            menu->currentItem = 0;
        }
    }
    if (btn == BTN_B_SHORT)
    {
        menu->items[menu->currentItem].functionPointer();
    }

    for (int i = 0; i < menu->itemCount; i++)
    {
        set_pos(4, MENU_ITEM_POS_Y(i));
        if (menu->currentItem == i)
        {
            set_fore_col(CHOOSEN_OPTION_COLOR);
            put_text(">");
        }
        else
        {
            put_text(" ");
        }
    }
}

static void drawMenu(struct Menu *menu)
{
    fill_scren(BLACK);
    plot(1, 1, RED);
    set_pos(4, 4);
    set_fore_col(YELLOW);
    put_text(menu->header);
    set_pos(4, 16);
    set_fore_col(YELLOW);
    put_text(menu->menuTitle);
    set_fore_col(WHITE);

    for (int i = 0; i < menu->itemCount; i++)
    {
        set_pos(16,  MENU_ITEM_POS_Y(i));
        put_text(menu->items[i].optionName);
    }
}