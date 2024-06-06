#include "menu.h"
#include "prompt.h"
#include "pico/stdlib.h"
#include "../util/buttons.h"
#include "../util/post_boot.h"
#include "../video_channel.h"

#define MENU_BOX_X 8
#define MENU_BOX_Y 6
#define MENU_BOX_W 367
#define MENU_BOX_H 275

void luma_calibration();
void calibration_diagram();
void chroma_calibration();
void toggle_chroma_decode_val();
void factory_reset();
void save_config();
void force_restart();
void luma_exit();
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
    {"Chroma calibration", chroma_calibration},
    {"Chroma calibration done", calibration_diagram},
    {"Chroma decode (experimental)", toggle_chroma_decode_val},
    {"Factory reset", factory_reset},
    {"Restart", force_restart},
    {"Save changes", save_config}};

struct Menu mainMenu = {
    .items = mainMenuItems,
    .itemCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]),
    .posX = 10,
    .posY = 60};

struct MenuItem lumaMenuItems[] = {
    {"Luma first delay value", luma_exit},
    {"Luma second delay value", luma_exit},
    {"Restore", luma_exit},
    {"Main menu", luma_exit}};

struct Menu lumaMenu = {
    .items = lumaMenuItems,
    .itemCount = sizeof(lumaMenuItems) / sizeof(lumaMenuItems[0])};

static inline void redraw_main_menu()
{
    fill_scren(BLACK);
    draw_text_box(&header);
    set_pos(mainMenu.posX, mainMenu.posY - 12);
    put_text("Main menu");
    draw_menu(&mainMenu);
}

static inline void update_chroma_decode_value()
{
    set_pos(300, menu_item_pos_y(&mainMenu, 2));
    if (app_cfg.enableChroma)
        put_text("ON ");
    else
        put_text("OFF");
}
static inline void update_chroma_calibration_done_value()
{
    set_pos(300, menu_item_pos_y(&mainMenu, 1));
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
        sleep_ms(20);
        update_menu(&mainMenu);
    }
}

bool luma_continue = true;

void luma_exit()
{
    luma_continue = false;
}

bool luma_calibration_handler()
{
    update_menu(&lumaMenu);
    if (luma_continue == false)
    {
        luma_continue = true;
        return false;
    }
    return true;
}

void luma_calibration()
{
    draw_menu(&lumaMenu);
    calibrate_luma(luma_calibration_handler);
    redraw_main_menu();
}

void do_chroma_calibration()
{
    fill_scren(BLACK);
    calibrate_chroma();
}
void calibration_diagram()
{
    fill_scren(BLACK);
    for(int dec=0; dec<2048; dec++){
        update_mapping_diagram(dec);
    }
    while (true)
    {
        tight_loop_contents();
    }
    
}    
void chroma_calibration()
{
    fill_scren(BLACK);
    draw_text_box(&header);
    set_pos(mainMenu.posX, mainMenu.posY - 12);
    put_text("Chroma Calibration");

    text_block_init(MENU_BOX_X + 8, mainMenu.posY, 46, 20);
    text_block_println("To perform proper calibration, follow these");
    text_block_println("steps:");
    text_block_println("1) switch to Atari screen.");
    text_block_println("2) load and run the CALSCR.BAS program");
    text_block_println("3) wait for it to execute");
    text_block_println("4) go back to this menu and confirm");
    text_block_println("   the calibration");

    show_prompt(do_chroma_calibration, main_menu_show, MENU_BOX_X + 8, MENU_BOX_Y + 36 + 80);
}

void toggle_chroma_decode_val()
{
    if (preset_loaded)
    {
        app_cfg.enableChroma = !app_cfg.enableChroma;
        update_chroma_decode_value();
    }
}

void save_config()
{
    set_post_boot_action(WRITE_CONFIG);
    force_restart();
}

void prompt(CallbackFunction onYes, CallbackFunction onNo)
{
    struct TextBox prompt = {
        .text = "Are you sure ?",
        .textColor = YELLOW,
        .frameColor = RED,
        .center = true,
        .posX = MENU_BOX_X + 70,
        .posY = MENU_BOX_H / 2,
        .width = MENU_BOX_W - 140,
        .height = 64};
    draw_text_box(&prompt);
    show_prompt(onYes, onNo, prompt.posX + 8, prompt.posY + 20);
}
void do_factory_reset()
{
    set_post_boot_action(FACTORY_RESET);
    force_restart();
}

void factory_reset()
{
    prompt(do_factory_reset, main_menu_show);
}

void force_restart()
{
    watchdog_enable(1, 1);
}

void none() {}