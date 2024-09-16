#include "flash_utils.h"
#include "hardware/sync.h"

void init_mappings(gc_n64_mapping* mappings){
    //TODO actually pull from flash
    int addr = XIP_BASE + MAPPING_TARGET_OFFSET;
    uint8_t *ptr = (uint8_t *) addr;
    if(ptr[0] == FLAG){
        addr++;
        gc_n64_mapping *mapping_ptr = (gc_n64_mapping *) addr;
        for(int i = 0; i < MAX_MAPPINGS; i++){
            memcpy(&mappings[i], &mapping_ptr[i], sizeof(gc_n64_mapping));
        }
    }
    else{
        for(int i = 0; i < MAX_MAPPINGS; i++){
            memcpy(&mappings[i], &default_mapping, sizeof(gc_n64_mapping));
        }
        save_mappings(mappings);
    }

}

void save_mappings(gc_n64_mapping* mappings){
    uint8_t buff[512];
    buff[0] = FLAG;
    for(int i = 0; i < MAX_MAPPINGS; i++){
        memcpy(&buff[1 + (i * sizeof(gc_n64_mapping))], &mappings[i], sizeof(gc_n64_mapping));
    }

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(MAPPING_TARGET_OFFSET, FLASH_PAGE_SIZE);
    flash_range_program(MAPPING_TARGET_OFFSET, buff, FLASH_PAGE_SIZE * 2);
    restore_interrupts(ints);
}