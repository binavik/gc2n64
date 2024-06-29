#include "common.h"
#include "n64.h"


void startN64(uint8_t* n64_status, bool &read, State gc_state, uint &offset){
    PIO pio = pio0;
    pio_sm_config config;
    joybus_program_init(pio, offset + joybus_offset_joybusout, N64_PIN, config, 8, 0);
    pio_sm_clear_fifos(pio, 1);
    while(true){

    }
}