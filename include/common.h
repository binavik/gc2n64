#ifndef common
#define common

#define DEBUG 1

#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "hardware/pio.h"
#if DEBUG
#include <stdio.h>
#include <string.h>
#endif

#define ALIGNED_JOYBUS_8(val) ((val) << 24)
#define SKIP {3, 7}

enum State{
    DISCONNECTED = 0,   //no controller is connected
    ZERO = 1,           //first iteration after connecting, requires zeroing
    CONNECTED = 2,      //connected and zeroed, poll normally
    RECALIBRATE = 3     //recenter controller
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

#endif