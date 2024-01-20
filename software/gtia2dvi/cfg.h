

#ifndef CFG_H
#define CFG_H

// #define PALETTE TRUE
#define FRAME_WIDTH (768/2)
#define FRAME_HEIGHT (576/2)

#define FIRST_ROW_TO_SHOW 40
#define NO_ROWS_TO_SHOW 248


#define VREG_VSEL VREG_VOLTAGE_1_30

#define LED_PIN 25

#define FORCE_MENU false

#define INVALID_CHROMA_HANDLER (gtia_palette[luma])
// #define INVALID_CHROMA_HANDLER (YELLOW)

struct AppConfig
{
    bool enableChroma;
    bool a1;
    bool a2;
    bool a3;
    uint32_t pad;
};

struct AppConfig appcfg __attribute__((section(".uninitialized_data")));

uint8_t fab2col[2048][2] __attribute__((section(".uninitialized_data")));


void cfg_init()
{
    appcfg.enableChroma = false;
}

bool preset_loaded = false;

#endif