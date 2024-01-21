#include "dvi.h"
#include "dvi_serialiser.h"
#include "dvi_timing.h"
#include "common_dvi_pin_configs.h"
#include "pico/multicore.h"
#include "buttons.h"

extern const struct dvi_timing dvi_timing_768x576p_50hz;

#define __dvi_const(x) __not_in_flash_func(x)
const struct dvi_timing __dvi_const(dvi_timing_768x576p_50hz) = {
	.h_sync_polarity   = false,
	.h_front_porch     = 36,
	.h_sync_width      = 72,
	.h_back_porch      = 96,
	.h_active_pixels   = 768,

	.v_sync_polarity   = true,
	.v_front_porch     = 3,	
	.v_sync_width      = 4,
	.v_back_porch      = 13,  
	.v_active_lines    = 576,

	.bit_clk_khz       = 285000
};

#define DVI_TIMING dvi_timing_768x576p_50hz
// #define DVI_TIMING dvi_timing_800x480p_60hz

struct dvi_inst dvi0;

static void __not_in_flash_func(core1_main)()
{
	dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
	dvi_start(&dvi0);
	dvi_scanbuf_main_16bpp(&dvi0);
	__builtin_unreachable();
}
	static uint scanline = 2;
static void __not_in_flash_func(core1_scanline_callback)()
{
	// Discard any scanline pointers passed back
	uint16_t *bufptr;
	while (queue_try_remove_u32(&dvi0.q_colour_free, &bufptr))
		;
	// // Note first two scanlines are pushed before DVI start

	bufptr = &framebuf[FRAME_WIDTH * (scanline)];
	queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
	scanline = (scanline + 1) % FRAME_HEIGHT;

	btn_debounce();
}

static void  __not_in_flash_func(setup_display)() {
#ifdef RUN_FROM_CRYSTAL
	set_sys_clock_khz(12000, true);
#else
	// Run system at TMDS bit clock
	set_sys_clock_khz(DVI_TIMING.bit_clk_khz, false);
#endif
	dvi0.timing = &DVI_TIMING;
	dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
	dvi0.scanline_callback = core1_scanline_callback;
	dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

	uint16_t *bufptr = framebuf;
	queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
	bufptr += FRAME_WIDTH;
	queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);

	multicore_launch_core1(core1_main);
}
