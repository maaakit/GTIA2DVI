#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "cfg.h"
#include "gfx/font8x8.h"
#include "gfx/gfx.h"
#include "chroma.h"
#include "util/flash_storage.h"
#include "dvi_display.h"
#include "video_channel.h"
#include "util/buttons.h"
#include "ui/app_menu.h"
#include "util/post_boot.h"
#include "util/uart_log.h"
#include "clock_sync.h"

int main()
{
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);

	uart_log_init();
	uart_log_putln("");
	uart_log_putln("/---------------\\");
	uart_log_putln("| GTIA2DVI_PICO |");
	uart_log_putln("\\---------------/");
	uart_log_putln("starting");
	uart_log_putln("about to exec post boot action");
	uart_log_flush_blocking();

	exec_post_boot_action();

	btn_init();

	if (flash_config_saved())
	{
		uart_log_putln("config loaded");
		flash_load_config(&app_cfg);
	}
	else
	{
		uart_log_putln("using default config");
		cfg_init();
	}

	setup_atari_clocks();
	measure_freq(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN0, "PAL");
	measure_freq(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN1, "OSC");

	if (flash_preset_saved())
	{
		uart_log_putln("preset loaded");
		flash_load_preset((uint8_t *)calibration_data);
		preset_loaded = true;
	}
	else
	{
		uart_log_putln("preset not loaded");
		app_cfg.enableChroma = false;
	}

	sleep_ms(100);
	bool menu_requested = btn_is_down(BTN_A);
	uart_log_putln("menu requested");
	uart_log_flush_blocking();

	sleep_ms(500);
	vreg_set_voltage(VREG_VSEL);
	sleep_ms(500);

	uart_log_putln("about to start dvi infrastructure");
	uart_log_flush_blocking();
	setup_display();

	if (menu_requested || FORCE_MENU)
	{
		uart_log_putln("entering menu");
		uart_log_flush_blocking();
		main_menu_show();
	}

	uart_log_putln("about to start gtia video stream");
	uart_log_flush_blocking();
	process_video_stream();

	__builtin_unreachable();
}
