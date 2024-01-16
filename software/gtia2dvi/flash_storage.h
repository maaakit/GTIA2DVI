
#include <stdint.h>
#include <stdbool.h>

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

bool preset_saved();
void load_preset(uint8_t *address);
void save_preset(uint8_t *address);

#endif  // FLASH_STORAGE_H