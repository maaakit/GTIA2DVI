#include "flash_storage.h"
#include <hardware/flash.h>
#include <string.h>
#include "pico/stdlib.h"

// PICO_FLASH_SIZE_BYTES # The total size of the RP2040 flash, in bytes
// FLASH_SECTOR_SIZE     # The size of one sector, in bytes (the minimum amount you can erase)
// FLASH_PAGE_SIZE       # The size of one page, in bytes (the mimimum amount you can write)

#define PRESET_NUMBER 14

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * PRESET_NUMBER)
#define PRESET_SIZE 4096

bool preset_saved()
{

    uint32_t *p, preset_adr, value;

    preset_adr = XIP_BASE + FLASH_TARGET_OFFSET;
    p = (uint32_t *)preset_adr;

    for (int i = 0; i < PRESET_SIZE / 4; i++)
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

void load_preset(uint8_t *address)
{
    memcpy(address, XIP_BASE + FLASH_TARGET_OFFSET, PRESET_SIZE);
}

void save_preset(uint8_t *address)
{
    set_sys_clock_khz(125000, true);

    sleep_ms(500);

    flash_range_erase(FLASH_TARGET_OFFSET, PRESET_SIZE);

    sleep_ms(100);

    flash_range_program(FLASH_TARGET_OFFSET, address, PRESET_SIZE);

    watchdog_enable(1, 1);
}
