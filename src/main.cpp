#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "common.h"
#include "gamecube_controller.h"
#include "joybus.pio.h"
#include "n64.h"

mutex hold_flag;
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

static gc_n64_mapping default_mapping = {
    {3, 0},     //A
    {3, 1},     //B
    {2, 4},     //Z
    {3, 4},     //Start
    {2, 3},     //Dup
    {2, 2},     //Ddown
    {2, 0},     //Dleft
    {2, 1},     //Dright
    {2, 6},     //L
    {2, 5},     //R
    {0, 0},     //Cup
    {0, 0},     //Cdown
    {0, 0},     //Cleft
    {0, 0}      //Cright
};

static gc_n64_mapping c_usage = {
    {3, 0},     //A
    {3, 1},     //B
    {2, 4},     //Z
    {3, 4},     //Start
    {2, 3},     //Dup
    {2, 2},     //Ddown
    {2, 0},     //Dleft
    {2, 1},     //Dright
    SKIP,       //L
    {2, 5},     //R
    {0, 0},     //Cup
    {0, 0},     //Cdown
    {3, 3},     //Cleft
    {3, 2}      //Cright
};

static gc_n64_mapping *mappings[16] = {NULL};

void init(){
    
#if DEBUG
    stdio_init_all();
#endif
    mutex_init(&hold_flag);
    //todo, separate function to read any saved mappings from flash to the mappings array
    for(int i = 0; i < 0x0f; i++){
        mappings[i] = &default_mapping;
    }
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
    sleep_ms(10);
    startN64(n64_status);
    return 0; 
}