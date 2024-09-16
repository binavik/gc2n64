#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "common.h"
#include "gamecube_controller.h"
#include "joybus.pio.h"
#include "n64.h"
#include "flash_utils.h"

bool data_ready;
uint offset = pio_add_program(pio0, &joybus_program);

//global variable, 8 bytes from controller
//stored as 2, 32 bit uints, when broken down into a single byte array, the bytes and order are as follows
//byte 7: cstick_x
//byte 6: cstick_y
//byte 5: left_trigger
//byte 4: right_trigger
//byte 3: bits 0, 0, 0, Start, Y, X, B, A
//byte 2: bits 1, L, R, Z, Dup, Ddown, Dright, Dleft. Nintendo... why flip Dl and Dr? There's no reason for this
//byte 1: stick_x
//byte 0: stick_y

static uint32_t gc_status[2];

//global variable, 4 bytes mapped from gc_status
//byte 0: bits A, B, Z, Start, Dup, Ddown, Dleft, Dright
//byte 1: Reset, 0, L, R, Cup, Cdown, Cleft, Cright
//byte 2: stick_x
//byte 3: stick_y
static uint8_t n64_status[4] = {0x00, 0x00, 0x00, 0x00};

static State gc_state;


//this array is 448 bytes long, pico can only read from flash 256 bytes at a time, so 2 read need to be done
// to pull from flash
static gc_n64_mapping mappings[16];

void init(){
    
#if DEBUG
    stdio_init_all();
#endif
    //todo, separate function to read any saved mappings from flash to the mappings array
    init_mappings(mappings);
}

void core1(){
    startGC(gc_status, n64_status, gc_state, offset, mappings);
}

int main() {
    gc_state = DISCONNECTED;
    data_ready = false;
    init();
    sleep_ms(10);
    multicore_launch_core1(core1);
    for(;;){}
    sleep_ms(10);
    startN64(n64_status);
    return 0; 
}