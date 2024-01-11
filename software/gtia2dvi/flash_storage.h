
#include <stdint.h>
#include <stdbool.h>

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

int number_of_presets();
void load_preset(int pres_number, uint8_t *address);
bool save_preset(int pres_number, uint8_t *address);

#endif  // FLASH_STORAGE_H