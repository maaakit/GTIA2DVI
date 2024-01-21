

struct MenuItem prompt_menu_items[] = {
    {"No get me back", NULL},
    {"Yes, go forward", NULL}};

typedef void (*CallbackFunction)();

void show_prompt(CallbackFunction on_yes, CallbackFunction on_no, uint x, uint y)
{
    prompt_menu_items[0].functionPointer = on_no;
    prompt_menu_items[1].functionPointer = on_yes;

    struct Menu prompt_menu = {
        .items = prompt_menu_items,
        .itemCount = sizeof(prompt_menu_items) / sizeof(prompt_menu_items[0]),
        .posX = x,
        .posY = y};
    draw_menu(&prompt_menu);
    while (true)
    {
        sleep_ms(20);
        update_menu(&prompt_menu);
    }
}