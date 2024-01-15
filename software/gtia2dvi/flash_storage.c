#include "flash_storage.h"
#include <hardware/flash.h>
#include <string.h>
#include "pico/stdlib.h"

// PICO_FLASH_SIZE_BYTES # The total size of the RP2040 flash, in bytes
// FLASH_SECTOR_SIZE     # The size of one sector, in bytes (the minimum amount you can erase)
// FLASH_PAGE_SIZE       # The size of one page, in bytes (the mimimum amount you can write)

#define PRESET_NUMBER 8

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * PRESET_NUMBER)
#define PRESET_SIZE 4096

int number_of_presets()
{

    uint32_t *p, preset_adr, value;

    preset_adr = XIP_BASE + FLASH_TARGET_OFFSET;
    p = (uint32_t *)preset_adr;

    for (int i = 0; i < PRESET_SIZE / 4; i++)
    {
        value = *p;
        if (value != 0xffffffff)
        {
            return 1;
        }
        p++;
    }
    return 0;
}

void load_preset(int pres_number, uint8_t *address)
{
    memcpy(address, XIP_BASE + FLASH_TARGET_OFFSET, PRESET_SIZE);
}

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))



bool save_preset(int pres_number, uint8_t *address)
{

    set_sys_clock_khz(125000, true);

    flash_range_program(FLASH_TARGET_OFFSET, address, PRESET_SIZE);

    uint8_t *preset_adr = XIP_BASE + FLASH_TARGET_OFFSET;
    for (int i = 0; i < PRESET_SIZE; ++i)
    {
        if (address[i] != preset_adr[i])
            return false;
    }
    // RP2040 reset
    AIRCR_Register = 0x5FA0004;
}

