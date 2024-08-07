#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "cfg.h"
#include "gfx/font8x8.h"
#include "gfx/gfx.h"
#include "util/flash_storage.h"
#include "dvi_display.h"
#include "video_channel.h"
#include "util/buttons.h"
#include "ui/app_menu.h"
#include "util/post_boot.h"
#include "util/uart_log.h"
#include "gtia_dump.h"
#include "git_info.h"
#include "gtia_pal_gen.pio.h"

int main()
{
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);

	uart_log_init();
	uart_log_putln("");
	uart_log_putln("/---------------\\");
	uart_log_putln("| GTIA2DVI_PICO |");
	uart_log_putln("\\---------------/");
	uart_log_putln(get_commit_info(buf, 128));
	uart_log_putln("starting");
	uart_log_putln("about to exec post boot action");
	uart_log_flush_blocking();

	exec_post_boot_action();

	btn_init();

	sleep_ms(10);

	if (gtia_dump_is_requested() || GTIA_DUMP_FORCE)
	{
		uart_log_putln("GTIA DUMP mode requested");
		uart_log_flush_blocking();
		gtia_dump_process();
	}

	bool menu_requested = btn_is_down(BTN_A);

	if (flash_config_saved())
	{
		uart_log_putln("config loaded");
		flash_load_config(&app_cfg);
		cfg_log_to_uart();
	}
	else
	{
		uart_log_putln("using default config");
		cfg_init();
		cfg_log_to_uart();
	}

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

	uart_log_flush_blocking();

	sleep_ms(200);
	vreg_set_voltage(VREG_VSEL);
	sleep_ms(200);

	uart_log_putln("about to start dvi infrastructure");
	uart_log_flush_blocking();

	sleep_ms(100);
	setup_display();


	 uint o = pio_add_program(pio0, &gtia_pal_gen_program);
     gtia_pal_gen_program_init(pio0, 3, o);

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
