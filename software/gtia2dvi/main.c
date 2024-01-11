#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "cfg.h"
#include "gfx.h"
#include "chroma.h"
#include "flash_storage.h"
#include "dvi_display.h"
#include "video_channel.h"


int __not_in_flash_func(main)()
{
	bool preset_loaded = false;
	if (number_of_presets() > 0)
	{
		load_preset(0, (uint8_t *)fab2col);
		preset_loaded = true;
	}
	sleep_ms(2000);
	vreg_set_voltage(VREG_VSEL);
	sleep_ms(1000);

	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);

	setup_display();

	if (preset_loaded) {
		process_video_stream();
	} else {
		calibrate_chroma();
	}

	__builtin_unreachable();
}
