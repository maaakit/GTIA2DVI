//#define PALETTE TRUE
#define FRAME_WIDTH 400
#define FRAME_HEIGHT 240
#define VREG_VSEL VREG_VOLTAGE_1_30
#define DVI_TIMING dvi_timing_800x480p_60hz
//#define DVI_TIMING dvi_timing_800x480p_50hz
//#define DVI_TIMING dvi_timing_640x480p_50hz
#define LED_PIN 25


#define FORCE_MENU true

//#define IGNORE_CHROMA


#define INVALID_CHROMA_HANDLER (gtia_palette[luma])
//#define INVALID_CHROMA_HANDLER (YELLOW)