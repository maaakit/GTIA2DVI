

struct MenuItem promptMenuItems[] = {
    {"No get me back", NULL},
    {"Yes, go forward", NULL}};

typedef void (*CallbackFunction)();

void showPrompt(CallbackFunction onYes, CallbackFunction onNo, uint x, uint y)
{
    promptMenuItems[0].functionPointer = onNo;
    promptMenuItems[1].functionPointer = onYes;

    struct Menu promptMenu = {
        .items = promptMenuItems,
        .itemCount = sizeof(promptMenuItems) / sizeof(promptMenuItems[0]),
        .posX = x,
        .posY = y};
    drawMenu(&promptMenu);
    while (true)
    {
        sleep_us(20 * 1000);
        updateMenu(&promptMenu);
    }
}