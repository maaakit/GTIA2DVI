#include "buttons.h"
#include "video_channel.h"
#include "menu.h"
#include "pico/stdlib.h"

void systemInfo();
void lumaCalibration();
void chromaCalibration();
void option4();
void factoryReset();
void restart();
void lumaExit();

struct MenuItem mainMenuItems[] = {
    {"System Info", systemInfo},
    {"Luma calibration", lumaCalibration},
    {"Chroma calibration", chromaCalibration},
    {"Enable chroma (experimental)", option4},
    {"Factory reset", factoryReset},
    {"Restart", restart}};

struct Menu mainMenu = {
    .header = "ATARI GTIA2DVI MENU",
    .menuTitle = "Main Menu",
    .items = mainMenuItems,
    .itemCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0])
};


struct MenuItem lumaMenuItems[] = {
    {"Luma first delay value", lumaExit},
    {"Increase", restart},
    {"Decrease", restart},
    {"Luma second delay value", lumaExit},
    {"Increase", restart},
    {"Decrease", restart},
    {"Save", restart},
    {"Cancel", lumaExit}
};
struct Menu lumaMenu = {
    .header = "ATARI GTIA2DVI MENU",
    .menuTitle = "Luma Calibration",
    .items = lumaMenuItems,
    .itemCount = sizeof(lumaMenuItems) / sizeof(lumaMenuItems[0])
};

static inline void main_menu_show()
{
    btn_init();
    drawMenu(&mainMenu);

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

void lumaExit(){
    lumaContinue = false;
}

bool lumaCalibrationHandler()
{
    updateMenu(&lumaMenu);
    if (lumaContinue ==false){
        lumaContinue=true;
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
    set_pos(10, 150);
    put_text("chroma calibration");
    calibrate_chroma();
}

void option4()
{
    put_text("enable chroma");
}

void factoryReset() {}
void restart() {}