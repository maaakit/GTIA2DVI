#include "../gfx/gfx_text.h"

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

static inline int menu_item_pos_y(struct Menu *menu, int item)
{
    return item * 10 + menu->posY;
}

static inline void update_menu(struct Menu *menu)
{
    int btn = btn_last_event();

    if (btn_event_index(btn) == BTN_A)
    {
        menu->currentItem++;
        if (menu->currentItem >= menu->itemCount)
        {
            menu->currentItem = 0;
        }
    }
    if (btn_event_index(btn) == BTN_B)
    {
        menu->items[menu->currentItem].functionPointer();
    }

    for (int i = 0; i < menu->itemCount; i++)
    {
        set_pos(menu->posX, menu_item_pos_y(menu, i));
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

static void draw_text_box(struct TextBox *tb)
{
    box(tb->posX, tb->posY, tb->width, tb->height, tb->frameColor);

    set_fore_col(tb->textColor);
    if (tb->center)
    {
        set_pos(tb->posX + (tb->width / 2) - strlen(tb->text) * 8 / 2, tb->posY + 4);
    }
    put_text(tb->text);
}

static void draw_menu(struct Menu *menu)
{
    set_pos(menu->posX, menu->posY);
    set_fore_col(WHITE);

    for (int i = 0; i < menu->itemCount; i++)
    {
        set_pos(menu->posX + 16, menu_item_pos_y(menu, i));
        put_text(menu->items[i].optionName);
    }
}