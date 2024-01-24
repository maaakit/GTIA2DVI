#include "flash_storage.h"

#ifndef POST_BOOT_H
#define POST_BOOT_H

enum PostBootAction
{
    FACTORY_RESET = (1 << 0),
    WRITE_PRESET = (1 << 1),
    WRITE_CONFIG = (1 << 2),
    START_CHROMA_CALIBRATION = (1 << 3)
};

enum PostBootAction post_boot_action __attribute__((section(".uninitialized_data")));

void set_post_boot_action(enum PostBootAction action)
{
    post_boot_action |= action;
}

void exec_post_boot_action()
{
    if (watchdog_caused_reboot() == false)
    {
        post_boot_action = 0;
        return;
    }

    if (post_boot_action & FACTORY_RESET)
    {
        flash_factory_reset();
    }
    if (post_boot_action & WRITE_PRESET)
    {
        flash_save_preset((uint8_t *)&calibration_data);
    }
    if (post_boot_action & WRITE_CONFIG)
    {
        flash_save_config(&app_cfg);
    }

    post_boot_action = 0;
}

#endif