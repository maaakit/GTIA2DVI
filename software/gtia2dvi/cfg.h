

#ifndef CFG_H
#define CFG_H

// #define PALETTE TRUE
#define FRAME_WIDTH 400
#define FRAME_HEIGHT 240
#define VREG_VSEL VREG_VOLTAGE_1_30
#define DVI_TIMING dvi_timing_800x480p_60hz
// #define DVI_TIMING dvi_timing_800x480p_50hz
// #define DVI_TIMING dvi_timing_640x480p_50hz
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

#endif