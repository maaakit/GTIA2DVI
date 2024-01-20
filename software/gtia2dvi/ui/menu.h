#include "../gfx_text.h"
#include "../gfx.h"

#define CHOOSEN_OPTION_COLOR GREEN
#define ACTIVE_OPTION_COLOR WHITE
#define DISABLED_OPTION_COLOR CYAN



struct MenuItem
{
    char *optionName;
    void (*functionPointer)();
};

struct Menu
{
    struct MenuItem *items;
    int itemCount;
    int currentItem;
    int posX;
    int posY;
};

struct TextBox
{
    const char *text;
    const uint16_t textColor;
    bool center;

    int posX;
    int posY;
    int width;
    int height;
    const uint16_t frameColor;
};

static inline int menuItemPosY(struct Menu *menu, int item)
{
    return item * 10 + menu->posY;
}

static inline void updateMenu(struct Menu *menu)
{
    enum btn_event btn = btn_last_event();

    if (btn == BTN_A_SHORT)
    {
        menu->currentItem++;
        //   plot(1,menu->currentItem+10,GREEN);
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
        set_pos(menu->posX, menuItemPosY(menu, i));
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

static void drawTextBox(struct TextBox *tb){
    box(tb->posX, tb->posY, tb->width, tb->height, tb->frameColor);

    set_fore_col(tb->textColor);
    if ( tb->center) {
       set_pos(tb->posX + (tb->width / 2) - strlen(tb->text) * 8 / 2, tb->posY + 4);
    }
    put_text(tb->text);
}

static void drawMenu(struct Menu *menu)
{
    set_pos(menu->posX , menu->posY );
    set_fore_col(WHITE);

    for (int i = 0; i < menu->itemCount; i++)
    {
        set_pos(menu->posX + 16, menuItemPosY(menu, i));
        put_text(menu->items[i].optionName);
    }
}