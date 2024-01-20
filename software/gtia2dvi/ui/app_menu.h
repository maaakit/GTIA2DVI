#include "../buttons.h"
#include "../video_channel.h"
#include "../post_boot.h"
#include "menu.h"
#include "prompt.h"
#include "pico/stdlib.h"

#define MENU_BOX_X 8
#define MENU_BOX_Y 6
#define MENU_BOX_W 367
#define MENU_BOX_H 275

void systemInfo();
void lumaCalibration();
void chromaCalibration();
void toggleChromaDecode();
void factoryReset();
void saveConfig();
void force_restart();
void lumaExit();
void none();

struct TextBox header = {
    .text = "ATARI GTIA2DVI MENU",
    .textColor = YELLOW,
    .frameColor = GREEN,
    .center = true,
    .posX = MENU_BOX_X,
    .posY = MENU_BOX_Y,
    .width = MENU_BOX_W,
    .height = 32};

struct MenuItem mainMenuItems[] = {
    {"Chroma calibration", chromaCalibration},
    {"Chroma calibration done", none},
    {"Chroma decode (experimental)", toggleChromaDecode},
    {"Factory reset", factoryReset},
    {"Restart", force_restart},
    {"Save changes", saveConfig}};

struct Menu mainMenu = {
    .items = mainMenuItems,
    .itemCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]),
    .posX = 10,
    .posY = 60};

struct MenuItem lumaMenuItems[] = {
    {"Luma first delay value", lumaExit},
    {"Luma second delay value", lumaExit},
    {"Restore", lumaExit},
    {"Main menu", lumaExit}};

struct Menu lumaMenu = {
    .items = lumaMenuItems,
    .itemCount = sizeof(lumaMenuItems) / sizeof(lumaMenuItems[0])};

static inline void redraw_main_menu()
{
    fill_scren(BLACK);
    drawTextBox(&header);
    set_pos(mainMenu.posX, mainMenu.posY - 12);
    put_text("Main menu");
    drawMenu(&mainMenu);
}

static inline void update_chroma_decode_value()
{
    set_pos(300, menuItemPosY(&mainMenu, 2));
    if (appcfg.enableChroma)
        put_text("ON ");
    else
        put_text("OFF");
}
static inline void update_chroma_calibration_done_value()
{
    set_pos(300, menuItemPosY(&mainMenu, 1));
    if (preset_loaded)
        put_text("YES");
    else
        put_text("NO");
}

static inline void main_menu_show()
{
    redraw_main_menu();
    update_chroma_decode_value();
    update_chroma_calibration_done_value();
    while (true)
    {
        sleep_us(20 * 1000);
        updateMenu(&mainMenu);
    }
}

bool lumaContinue = true;

void lumaExit()
{
    lumaContinue = false;
}

bool lumaCalibrationHandler()
{
    updateMenu(&lumaMenu);
    if (lumaContinue == false)
    {
        lumaContinue = true;
        return false;
    }
    return true;
}

void lumaCalibration()
{
    drawMenu(&lumaMenu);
    calibrate_luma(lumaCalibrationHandler);
    redraw_main_menu();
}

void doChromaCalibration()
{
    fill_scren(BLACK);
    calibrate_chroma();
}

void chromaCalibration()
{
    fill_scren(BLACK);
    drawTextBox(&header);
    set_pos(mainMenu.posX, mainMenu.posY - 12);
    put_text("Chroma Calibration");

 //   box(MENU_BOX_X, MENU_BOX_Y + 32, MENU_BOX_W, MENU_BOX_H - 32, GREEN);

    text_block_init(MENU_BOX_X + 8, mainMenu.posY, 46, 20);
    text_block_println("To perform proper calibration, follow these");
    text_block_println("steps:");
    text_block_println("1) switch to Atari screen.");
    text_block_println("2) load and run the CALSCR.BAS program");
    text_block_println("3) wait for it to execute");
    text_block_println("4) go back to this menu and confirm");
    text_block_println("   the calibration");

    showPrompt(doChromaCalibration, main_menu_show, MENU_BOX_X + 8, MENU_BOX_Y + 36 + 80);
}

void toggleChromaDecode()
{
    if (preset_loaded)
    {
        appcfg.enableChroma = !appcfg.enableChroma;
        update_chroma_decode_value();
    }
}

void saveConfig()
{
    setPostBootAction(WRITE_CONFIG);
    force_restart();
}

void prompt(CallbackFunction onYes, CallbackFunction onNo)
{
    struct TextBox propmpt = {
        .text = "Are you sure ?",
        .textColor = YELLOW,
        .frameColor = RED,
        .center = true,
        .posX = MENU_BOX_X + 70,
        .posY = MENU_BOX_H / 2,
        .width = MENU_BOX_W - 140,
        .height = 64};
    drawTextBox(&propmpt);
    showPrompt(onYes, onNo, propmpt.posX + 8, propmpt.posY + 20);
}
void doFactoryReset()
{
    setPostBootAction(FACTORY_RESET);
    force_restart();
}

void factoryReset()
{
    prompt(doFactoryReset, main_menu_show);
}

void force_restart()
{
    watchdog_enable(1, 1);
}

void none() {}