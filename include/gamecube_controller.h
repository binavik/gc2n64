#ifndef gcc
#define gcc

#define MAX_TRIGGER 0xb0
#define MAX_DISCONNECTS 10

#define GC_PIN 18
#define GCC_SM 0
#define MAX_NEGATIVE 0x50
#define MAX_POSITIVE 0xb0

enum Mapping_State{
    CHECK_MAPPING = 0,
    HOLD_NEW_MAPPING = 1,
    CHANGE_MAPPING = 2
};

void startGC(uint32_t* gc_status, uint8_t* n64_status, State &gc_state, uint &offset, gc_n64_mapping* mappings);

#endif