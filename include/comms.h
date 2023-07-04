#ifndef comms
#define comms
#endif

#include "pico/mutex.h"

#define GC_ARR_SIZE 8
#define GC_PIN 1
#define N64_PIN 8

void startGC(uint8_t* gc_status, uint8_t* gc_zero, mutex_t mtx);
void startN64(uint8_t* gc_status, mutex_t mtx);