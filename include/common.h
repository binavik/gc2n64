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

enum State{
    DISCONNECTED = 0,   //no controller is connected
    ZERO = 1,           //first iteration after connecting, requires zeroing
    CONNECTED = 2       //connected and zeroed, poll normally
};

#endif