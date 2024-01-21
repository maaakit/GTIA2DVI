

#ifndef CFG_H
#define CFG_H

// #define PALETTE TRUE
#define FRAME_WIDTH (768/2)
#define FRAME_HEIGHT (576/2)

#define FIRST_ROW_TO_SHOW 40
#define ROWS_TO_SHOW 248

#define VREG_VSEL VREG_VOLTAGE_1_30

#define LED_PIN 25

#define FORCE_MENU false

#define INVALID_CHROMA_HANDLER (gtia_palette[luma])
// #define INVALID_CHROMA_HANDLER (YELLOW)

struct AppConfig
{
    bool enableChroma;
    uint32_t pad;
};


uint8_t calibration_data[2048][2] __attribute__((section(".uninitialized_data")));

struct AppConfig app_cfg __attribute__((section(".uninitialized_data")));

void cfg_init()
{
    app_cfg.enableChroma = false;
}

bool preset_loaded = false;

#endif