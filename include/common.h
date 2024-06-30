#ifndef common
#define common

#define DEBUG 1

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "joybus.pio.h"
#if DEBUG
#include <stdio.h>
#include <string.h>
#endif

#define ALIGNED_JOYBUS_8(val) ((val) << 24)
#define SKIP {3, 7}

enum State{
    DISCONNECTED = 0,   //no controller is connected
    ZERO = 1,           //first iteration after connecting, requires zeroing
    CONNECTED = 2       //connected and zeroed, poll normally
};

struct button{
    uint8_t byte;
    uint8_t bit;
};

typedef struct{
    button A;
    button B;
    button Z;
    button Start;
    button Dup;
    button Ddown;
    button Dleft;
    button Dright;
    button L;
    button R;
    button Cup;
    button Cdown;
    button Cleft;
    button Cright;
}gc_n64_mapping;

#endif