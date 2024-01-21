
#include <stdint.h>
#include <stdbool.h>
#include "cfg.h"
#include <hardware/flash.h>
#include <hardware/watchdog.h>
#include <hardware/sync.h>
#include <string.h>
#include "pico/stdlib.h"

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

// PICO_FLASH_SIZE_BYTES # The total size of the RP2040 flash, in bytes
// FLASH_SECTOR_SIZE     # The size of one sector, in bytes (the minimum amount you can erase)
// FLASH_PAGE_SIZE       # The size of one page, in bytes (the mimimum amount you can write)

#define PRESET_SECTOR 0
#define CONFIG_SECTOR 1
#define PRESET_SIZE 4096

#define FLASH_SECTOR_OFFSET(o) (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * (o) - FLASH_SECTOR_SIZE )

bool _is_sector_empty(uint32_t *adr)
{
    uint32_t *p, value;

    p = (uint32_t *)adr;

    for (int i = 0; i < FLASH_SECTOR_SIZE / 4; i++)
    {
        value = *p;
        if (value != 0xffffffff)
        {
            return true;
        }
        p++;
    }
    return false;
}

void _save_data(uint32_t flash_offs, const uint8_t *data, size_t count)
{
    flash_range_erase(flash_offs, count);
    flash_range_program(flash_offs, data, count);
}

void flash_factory_reset()
{
    flash_range_erase(FLASH_SECTOR_OFFSET(CONFIG_SECTOR), FLASH_SECTOR_SIZE);
    flash_range_erase(FLASH_SECTOR_OFFSET(PRESET_SECTOR), FLASH_SECTOR_SIZE);
}

bool flash_preset_saved()
{
    return _is_sector_empty(XIP_BASE + FLASH_SECTOR_OFFSET(PRESET_SECTOR));
}

void flash_load_preset(uint8_t *address)
{
    memcpy(address, XIP_BASE + FLASH_SECTOR_OFFSET(PRESET_SECTOR), PRESET_SIZE);
}

void flash_save_preset(uint8_t *address)
{
    _save_data(FLASH_SECTOR_OFFSET(PRESET_SECTOR), address, PRESET_SIZE);
}

bool flash_config_saved()
{
    return _is_sector_empty(XIP_BASE + FLASH_SECTOR_OFFSET(CONFIG_SECTOR));
}

void flash_load_config(struct AppConfig *address)
{
    memcpy(address, XIP_BASE + FLASH_SECTOR_OFFSET(CONFIG_SECTOR), sizeof(struct AppConfig));
}

void flash_save_config(struct AppConfig *address)
{
    _save_data(FLASH_SECTOR_OFFSET(CONFIG_SECTOR), (uint8_t*) address, FLASH_SECTOR_SIZE);
}

#endif // FLASH_STORAGE_H