#ifndef flash_utils
#define flash_utils

#include "common.h"
#include "hardware/flash.h"

#define MAPPING_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLAG 0x45
#define MAX_MAPPINGS 16

void init_mappings(gc_n64_mapping* mappings);
void save_mappings(gc_n64_mapping* mappings);


#endif