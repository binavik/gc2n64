#ifndef comms
#define comms
#endif

//set to 0 to disable serial output over USB
#define DEBUG 0
#define MAX_STICK 82
#define MAX_TRIGGER 0xb0

#define WATCHDOG_DELAY_MS 100 
#define RESPONSE_DELAY  3

#define GC_ARR_SIZE 8
#define GC_PIN 1
#define N64_PIN 18

#define MAX_NEGATIVE 0x50
#define MAX_POSITIVE 0xb0

struct mapping{
    uint8_t A_BYTE;
    uint8_t A_BIT;
    uint8_t B_BYTE;
    uint8_t B_BIT;
    uint8_t Z_BYTE;
    uint8_t Z_BIT;
    uint8_t START_BYTE;
    uint8_t START_BIT;
    uint8_t D_UP_BYTE;
    uint8_t D_UP_BIT;
    uint8_t D_DOWN_BYTE;
    uint8_t D_DOWN_BIT;
    uint8_t D_LEFT_BYTE;
    uint8_t D_LEFT_BIT;
    uint8_t D_RIGHT_BYTE;
    uint8_t D_RIGHT_BIT;
    uint8_t L_BYTE;
    uint8_t L_BIT;
    uint8_t R_BYTE;
    uint8_t R_BIT;
};

//button mapping
#define GC_START_BYTE       0
#define GC_START_BIT        4
#define GC_Y_BYTE           0
#define GC_Y_BIT            3
#define GC_X_BYTE           0
#define GC_X_BIT            2
#define GC_B_BYTE           0
#define GC_B_BIT            1
#define GC_A_BYTE           0
#define GC_A_BIT            0
#define GC_L_BYTE           1
#define GC_L_BIT            6
#define GC_R_BYTE           1
#define GC_R_BIT            5
#define GC_Z_BYTE           1
#define GC_Z_BIT            4
#define GC_D_UP_BYTE        1
#define GC_D_UP_BIT         3
#define GC_D_DOWN_BYTE      1
#define GC_D_DOWN_BIT       2
#define GC_D_RIGHT_BYTE     1
#define GC_D_RIGHT_BIT      1
#define GC_D_LEFT_BYTE      1
#define GC_D_LEFT_BIT       0

#define N64_A_BYTE          0
#define N64_A_BIT           7
#define N64_B_BYTE          0
#define N64_B_BIT           6
#define N64_Z_BYTE          0
#define N64_Z_BIT           5
#define N64_START_BYTE      0
#define N64_START_BIT       4
#define N64_D_UP_BYTE       0
#define N64_D_UP_BIT        3
#define N64_D_DOWN_BYTE     0
#define N64_D_DOWN_BIT      2
#define N64_D_LEFT_BYTE     0
#define N64_D_LEFT_BIT      1
#define N64_D_RIGHT_BYTE    0
#define N64_D_RIGHT_BIT     0
#define N64_L_BYTE          1
#define N64_L_BIT           5
#define N64_R_BYTE          1
#define N64_R_BIT           4
#define N64_C_UP_BYTE       1
#define N64_C_UP_BIT        3
#define N64_C_DOWN_BYTE     1
#define N64_C_DOWN_BIT      2
#define N64_C_LEFT_BYTE     1
#define N64_C_LEFT_BIT      1
#define N64_C_RIGHT_BYTE    1
#define N64_C_RIGHT_BIT     0

void startGC(uint8_t* gc_status, uint8_t* n64_status, bool &read);
void startN64(uint8_t* gc_status, uint8_t* n64_status, bool &read);
