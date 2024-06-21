#include "util/uart_log.h"

#ifndef CFG_H
#define CFG_H

#define FRAME_WIDTH (768 / 2)
#define FRAME_HEIGHT (576 / 2)

#define FIRST_GTIA_ROW_TO_SHOW 14
#define GTIA_ROWS_TO_SHOW 288
#define SCREEN_OFFSET_Y 0
#define SCREEN_OFFSET_X 14

#define VREG_VSEL VREG_VOLTAGE_1_25

#define LED_PIN 25

#define FORCE_MENU false
#define LOG_ENABLED

#define INVALID_CHROMA_HANDLER (luma)
// #define INVALID_CHROMA_HANDLER (YELLOW)

#define CALIBRATION_FIRST_BAR_CHROMA_INDEX 31

struct AppConfig
{
    bool enableChroma;
    uint chroma_calib_offset;
    uint32_t pad;
};

int8_t calibration_data[2][4][2048] __attribute__((section(".uninitialized_data")));

struct AppConfig app_cfg __attribute__((section(".uninitialized_data")));

void cfg_init()
{
    app_cfg.enableChroma = false;
    app_cfg.chroma_calib_offset = 0;
}

void cfg_log_to_uart()
{
    uart_log_putlnf("current config:");
    uart_log_putlnf("  enableChroma: %d", app_cfg.enableChroma);
    uart_log_putlnf("  chroma_calib_offset: %d", app_cfg.chroma_calib_offset);
    uart_log_flush_blocking();
}

bool preset_loaded = false;

#endif