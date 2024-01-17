#include "buttons.h"
#include "video_channel.h"
#include "menu.h"
#include "pico/stdlib.h"
#include "post_boot.h"

void systemInfo();
void lumaCalibration();
void chromaCalibration();
void chromaDecode();
void factoryReset();
void saveConfig();
void force_restart();
void lumaExit();

struct MenuItem mainMenuItems[] = {
 //   {"System Info", systemInfo},
 //   {"Luma calibration", lumaCalibration},
    {"Chroma calibration", chromaCalibration},
    {"Chroma decode (experimental)", chromaDecode},
    {"Factory reset", factoryReset},
    {"Restart", force_restart},
    {"Save changes", saveConfig}};

struct Menu mainMenu = {
    .header = "ATARI GTIA2DVI MENU",
    .menuTitle = "Main Menu",
    .items = mainMenuItems,
    .itemCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0])};

struct MenuItem lumaMenuItems[] = {
    {"Luma first delay value", lumaExit},
    {"Luma second delay value", lumaExit},
    {"Restore", lumaExit},
    {"Main menu", lumaExit}};
struct Menu lumaMenu = {
    .header = "ATARI GTIA2DVI MENU",
    .menuTitle = "Luma Calibration",
    .items = lumaMenuItems,
    .itemCount = sizeof(lumaMenuItems) / sizeof(lumaMenuItems[0])};

static inline void update_chroma_decode()
{
    set_pos(300, 3 * 10 + 30);
    if (appcfg.enableChroma)
        put_text("ON ");
    else
        put_text("OFF");
}

static inline void main_menu_show()
{
    btn_init();
    drawMenu(&mainMenu);
    update_chroma_decode();
    while (true)
    {
        sleep_us(20 * 1000);
        plotf(399, 200, btn_pushed(BTN_A) ? GREEN : RED);
        plotf(399, 202, btn_pushed(BTN_B) ? GREEN : RED);
        updateMenu(&mainMenu);
    }
}

void systemInfo()
{
    set_pos(10, 150);
    put_text("systemInfo");
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

void __not_in_flash_func(lumaCalibration)()
{
    drawMenu(&lumaMenu);
    calibrate_luma(lumaCalibrationHandler);
    drawMenu(&mainMenu);
}

void chromaCalibration()
{
    calibrate_chroma();
}

void chromaDecode()
{
    appcfg.enableChroma = !appcfg.enableChroma;
    update_chroma_decode();
}

void saveConfig(){
    setPostBootAction(WRITE_CONFIG);
    force_restart();
}


void factoryReset() {
    setPostBootAction(FACTORY_RESET);
    force_restart();
}

void force_restart()
{
    watchdog_enable(1, 1);
}