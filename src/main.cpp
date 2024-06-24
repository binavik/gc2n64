#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "common.h"
#include "gamecube_controller.h"
#include "n64.h"

bool read_flag;

//global variable, 8 bytes from controller
//byte 0: bits 0, 0, 0, Start, Y, X, B, A
//byte 1: bits 1, L, R, Z, Dup, Ddown, Dright, Dleft. Nintendo... why flip Dl and Dr? There's no reason for this
//byte 2: stick_x
//byte 3: stick_y
//byte 4: cstick_x
//byte 5: cstick_y
//byte 6: left_trigger
//byte 7: right_trigger
static uint32_t gc_status[2];

//Gamecube controller expects absolute values for sticks, with 0 being left/down and 255 being right/up
//GC still reports zero point so we can use it for the N64, just needs to be saved first
static uint8_t gc_zero[2];

//global variable, 4 bytes mapped from gc_status
//byte 0: bits A, B, Z, Start, Dup, Ddown, Dleft, Dright
//byte 1: Reset, 0, L, R, Cup, Cdown, Cleft, Cright
//byte 2: stick_x
//byte 3: stick_y
static uint8_t n64_status[4];
//N64 expects relative, or signed, values for the sticks.
//default, in the event no other mappings have been defined

void init(){
#if DEBUG
    stdio_init_all();
#endif
//may want to replace this with a mutex
    read_flag = true;
}

void core1(){
    startGC(gc_status, n64_status, read_flag);
}

int main() {
    init();
    core1();
    return 0; 
}