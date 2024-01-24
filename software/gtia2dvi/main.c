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

int main()
{
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);

	uart_log_init();

	exec_post_boot_action();

	btn_init();

	uart_log_put("TEST");
	uart_log_flush();

	if (flash_config_saved())
	{
		flash_load_config(&app_cfg);
	}
	else
	{
		cfg_init();
	}

	if (flash_preset_saved())
	{
		flash_load_preset((uint8_t *)calibration_data);
		preset_loaded = true;
	}
	else
	{
		app_cfg.enableChroma = false;
	}

	sleep_ms(100);
	bool menu_requested = btn_is_down(BTN_A);
	sleep_ms(500);
	vreg_set_voltage(VREG_VSEL);
	sleep_ms(500);

	setup_display();

	if (menu_requested || FORCE_MENU)
	{
		main_menu_show();
	}

	process_video_stream();

	__builtin_unreachable();
}
