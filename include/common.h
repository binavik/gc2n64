#ifndef common
#define common

#define ALIGNED_JOYBUS_8(val) ((val) << 24)

#define DEBUG 1

enum State{
    DISCONNECTED = 0,   //no controller is connected
    ZERO = 1,           //first iteration after connecting, requires zeroing
    CONNECTED = 2       //connected and zeroed, poll normally
};

#endif