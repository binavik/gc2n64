#ifndef gcc
#define gcc

#define MAX_TRIGGER 0xb0
#define RESPONSE_DELAY 3
#define MAX_DISCONNECTS 10

#define GC_ARR_SIZE 8
#define GC_PIN 18
#define MAX_NEGATIVE 0x50
#define MAX_POSITIVE 0xb0


void startGC(uint32_t* gc_status, uint8_t* n64_status, bool &read, State &gc_state, uint &offset, gc_n64_mapping* mappings);

#endif