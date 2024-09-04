#include "dvi.h"
#include "dvi_serialiser.h"
#include "dvi_timing.h"
#include "common_dvi_pin_configs.h"
#include "pico/multicore.h"
#include "util/buttons.h"
#include "util/uart_log.h"
#include "tmds_encode.h"
#include "tmds_table_256.h"
#include "gtia_palette.h"
#include "hardware/clocks.h"

extern const struct dvi_timing dvi_timing_768x576p_50hz;

#define __dvi_const(x) __not_in_flash_func(x)

const struct dvi_timing __dvi_const(dvi_timing_768x576p_50hz) = {
	.h_sync_polarity = false,
	.h_front_porch = 36,
	.h_sync_width = 72,
	.h_back_porch = 96,
	.h_active_pixels = 768,

	.v_sync_polarity = true,
	.v_front_porch = 3,
	.v_sync_width = 4,
	.v_back_porch = 13,
	.v_active_lines = 576,

	.bit_clk_khz = 285000};

#define DVI_TIMING dvi_timing_768x576p_50hz

struct dvi_inst dvi0;

__scratch_y("tmds_palette")
uint32_t tmds_palette[PALETTE_SIZE * 3];

static inline void prepare_tmds_palette()
{
	for (uint c = 0; c < PALETTE_SIZE; c++)
	{
		tmds_palette[c] = tmds_table_256[gtia_rgb888_palette[c] & 0xff];
		tmds_palette[c + PALETTE_SIZE] = tmds_table_256[(gtia_rgb888_palette[c] >> 8) & 0xff];
		tmds_palette[c + PALETTE_SIZE * 2] = tmds_table_256[(gtia_rgb888_palette[c] >> 16) & 0xff];
	}
}

void __not_in_flash_func(_tmds_encode_palette_data)(const uint8_t *pixbuf, uint32_t *symbuf, size_t n_pix, uint delta)
{
	while (n_pix--)
	{
		uint8_t pix = pixbuf[0];
		*symbuf++ = tmds_palette[pix + delta];
		pixbuf++;
	}
}

static inline void __not_in_flash_func(_prepare_scanline)(struct dvi_inst *inst, uint8_t *scanbuf)
{
	uint32_t *tmdsbuf;
	queue_remove_blocking_u32(&inst->q_tmds_free, &tmdsbuf);
	uint pixwidth = inst->timing->h_active_pixels;
	uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;

	_tmds_encode_palette_data(scanbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2, 0);
	_tmds_encode_palette_data(scanbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2, 256);
	_tmds_encode_palette_data(scanbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2, 512);

	queue_add_blocking_u32(&inst->q_tmds_valid, &tmdsbuf);
}

void __not_in_flash_func(dvi_core_main_loop)(struct dvi_inst *inst)
{
	uint y = 0;
	while (1)
	{
		uint8_t *scanbuf;
		queue_remove_blocking_u32(&inst->q_colour_valid, &scanbuf);
		_prepare_scanline(inst, scanbuf);
		queue_add_blocking_u32(&inst->q_colour_free, &scanbuf);
		++y;
		if (y == inst->timing->v_active_lines)
		{
			y = 0;
		}
	}
	__builtin_unreachable();
}

static void __not_in_flash_func(core1_main)()
{
	dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
	dvi_start(&dvi0);
	dvi_core_main_loop(&dvi0);
	__builtin_unreachable();
}
static uint scanline = 2;
static void __not_in_flash_func(core1_scanline_callback)()
{
	// Discard any scanline pointers passed back

	uint8_t *bufptr;
	while (queue_try_remove_u32(&dvi0.q_colour_free, &bufptr))
		;
	// // Note first two scanlines are pushed before DVI start

	bufptr = &framebuf[FRAME_WIDTH * (scanline)];
	queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
	scanline = (scanline + 1) % FRAME_HEIGHT;

	btn_debounce();
	uart_log_flush();
}

static void __not_in_flash_func(setup_display)()
{
#ifdef RUN_FROM_CRYSTAL
	set_sys_clock_khz(12000, true);
#else
	// Run system at TMDS bit clock
	set_sys_clock_khz(DVI_TIMING.bit_clk_khz, false);
	// update uart clock after sys_clock_change
	uart_update_clkdiv();
#endif
	prepare_tmds_palette();

	dvi0.timing = &DVI_TIMING;
	dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
	dvi0.scanline_callback = core1_scanline_callback;
	dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

	uint8_t *bufptr = framebuf;
	queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
	bufptr += FRAME_WIDTH;
	queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);

	multicore_launch_core1(core1_main);
}
