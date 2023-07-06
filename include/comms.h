#ifndef comms
#define comms
#endif

#include "pico/mutex.h"

#define GC_ARR_SIZE 8
#define GC_PIN 1
#define N64_PIN 8

void startGC(mutex_t mtx);
void startN64(mutex_t mtx);