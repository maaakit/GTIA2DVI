#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "cfg.h"
#include "font8x8.h"
#include "gfx.h"
#include "chroma.h"
#include "flash_storage.h"
#include "dvi_display.h"
#include "video_channel.h"
#include "buttons.h"
#include "ui/app_menu.h"
#include "post_boot.h"

int __not_in_flash_func(main)()
{
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);


	executePostBootAction();

	btn_init();


	if (flash_config_saved())
	{
		flash_load_config(&appcfg);
	} else {
		cfg_init();
	}



	if (flash_preset_saved())
	{
		flash_load_preset((uint8_t *)fab2col);
		preset_loaded = true;
	} else {
		appcfg.enableChroma = false;
	}

	sleep_ms(100);
	bool menu_requested = btn_is_down(BTN_A);
	sleep_ms(1000);
	vreg_set_voltage(VREG_VSEL);
	sleep_ms(1000);

	setup_display();

	if (menu_requested || FORCE_MENU)
	{
		main_menu_show();
	}

	process_video_stream();

	__builtin_unreachable();
}
