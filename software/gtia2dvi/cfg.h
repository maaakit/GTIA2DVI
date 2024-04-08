
#ifndef CFG_H
#define CFG_H

#define FRAME_WIDTH (768 / 2)
#define FRAME_HEIGHT (576 / 2)

#define FIRST_GTIA_ROW_TO_SHOW 14
#define GTIA_ROWS_TO_SHOW 288
#define SCREEN_OFFSET_Y 0
#define SCREEN_OFFSET_X 14

#define VREG_VSEL VREG_VOLTAGE_1_30

#define LED_PIN 25

#define FORCE_MENU false
#define LOG_ENABLED
#define DUMP_PIXEL_FEATURE_ENABLED


struct AppConfig
{
    bool enableChroma;
    uint32_t pad;
};

struct AppConfig app_cfg __attribute__((section(".uninitialized_data")));

// chroma calibration definition
#define SAMPLING_FRAMES     12
#define SAMPLE_Y_FIRST      70
#define SAMPLE_Y_LAST       155
#define SAMPLE_X_FIRST      39
#define SAMPLE_X_SIZE       9
#define COUNTS              226
#define MIN_CALIB_COUNT     0
#define MAX_SAMPLE          0x3ff

// chroma calibration data
int8_t chroma_table[(MAX_SAMPLE+1)][2] __attribute__((section(".uninitialized_data")));

// delays for aligning color data for even and odd lines
uint8_t delays[] = {39, 37};

void cfg_init()
{
    app_cfg.enableChroma = false;
}

bool preset_loaded = false;

#endif