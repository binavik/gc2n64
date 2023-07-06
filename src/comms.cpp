#include <stdio.h>
#include <string.h>
#include "hardware/watchdog.h"
#include "hardware/pio.h"
#include "joybus.pio.h"

#include "comms.h"

#define WATCHDOG_DELAY_MS 10

//global variable, 8 bytes from controller
//byte 0: bits 0, 0, 0, Start, Y, X, B, A
//byte 1: bits 1, L, R, Z, Dup, Ddown, Dright, Dleft. Nintendo... why flip Dl and Dr? There's no reason for this
//byte 2: stick_x
//byte 3: stick_y
//byte 4: cstick_x
//byte 5: cstick_y
//byte 6: left_trigger
//byte 7: right_trigger
static uint8_t gc_status[8];

//Gamecube controller expects absolute values for sticks, with 0 being left/down and 255 being right/up
//GC still reports zero point so we can use it for the N64, just needs to be saved first
static uint8_t gc_zero[2];

//global variable, 4 bytes mapped from gc_status
//byte 0: bits A, B, Z, Start, Dup, Ddown, Dleft, Dright
//byte 1: Reset, 0, L, R, Cup, Cdown, Cleft, Cright
//byte 2: stick_x
//byte 3: stick_y
static uint8_t n64_stat[4];
//N64 expects relative, or signed, values for the sticks.

void __time_critical_func(convertToPio2)(const uint8_t* command, const int len, uint32_t* result, int& resultLen){
    // PIO Shifts to the right by default
    // In: pushes batches of 8 shifted left, i.e we get [0x40, 0x03, rumble (the end bit is never pushed)]
    // Out: We push commands for a right shift with an enable pin, ie 5 (101) would be 0b11'10'11
    // So in doesn't need post processing but out does
    if (len == 0) {
        resultLen = 0;
        return;
    }
    resultLen = len/2 + 1;
    int i;
    for (i = 0; i < resultLen; i++) {
        result[i] = 0;
    }
    for (i = 0; i < len; i++) {
        for (int j = 0; j < 8; j++) {
            result[i / 2] += 1 << (2 * (8 * (i % 2) + j) + 1);
            result[i / 2] += (!!(command[i] & (0x80u >> j))) << (2 * (8 * (i % 2) + j));
        }
    }
    // End bit
    result[len / 2] += 3 << (2 * (8 * (len % 2)));
}

void __time_critical_func(startGC)(mutex_t mtx){
    PIO pio = pio0;
    pio_gpio_init(pio, GC_PIN);
    uint offset = pio_add_program(pio, &joybus_program);

    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, GC_PIN);
    sm_config_set_out_pins(&config, GC_PIN, 1);
    sm_config_set_set_pins(&config, GC_PIN, 1);
    sm_config_set_clkdiv(&config, 5);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);
    pio_sm_set_enabled(pio, 0, true);

    uint8_t gc_read_command[3] = {0x40, 0x03, 0x00};
    uint32_t result[3]; 
    int resultLen;
    convertToPio2(gc_read_command, 3, result, resultLen);

    //init controller and get zero point, do this once outside of read loop
    pio_sm_set_enabled(pio, 0, false);
    pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
    pio_sm_set_enabled(pio, 0, true);

    mutex_enter_blocking(&mtx);

    for(int i = 0; i < resultLen; i++){
        pio_sm_put_blocking(pio, 0, result[i]);
    }
    for(int i = 0; i < GC_ARR_SIZE; i++){
        gc_status[i] = pio_sm_get_blocking(pio, 0);
    }
    gc_zero[0] = gc_status[2];      //zero x
    gc_zero[1] = gc_status[3];      //zero y
    mutex_exit(&mtx);
    sleep_us(400);

    while(true){
        pio_sm_set_enabled(pio, 0, false);
        pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
        pio_sm_set_enabled(pio, 0, true);

        mutex_enter_blocking(&mtx);
        //fprintf(stderr, "core 0 in mutex\n");
        for(int i = 0; i < resultLen; i++){
            pio_sm_put_blocking(pio, 0, result[i]);
        }
        for(int i = 0; i < GC_ARR_SIZE; i++){
            gc_status[i] = pio_sm_get_blocking(pio, 0);
        }
        //fprintf(stderr, "%x %x %x %x %x %x %x %x\n", gc_status[0], gc_status[1], gc_status[2], gc_status[3], gc_status[4], gc_status[5], gc_status[6], gc_status[7]);
        //sleep_us(400);
        mutex_exit(&mtx);
        sleep_us(400);
    }
}

void __time_critical_func(startN64)(mutex_t mtx){
    while(true){
        uint8_t data[8];
        mutex_enter_blocking(&mtx);
        memcpy(data, gc_status, 8);
        memset(gc_status, 0, 8);
        //byte always has the MSB set, so if 0 then the
        //controller disconnected, reset the pico to load it back
        if(data[1] == 0){
            watchdog_enable(WATCHDOG_DELAY_MS, 0);
            while(1){}
        }
        else{
            fprintf(stderr, "%x %x %x %x %x %x %x %x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
            mutex_exit(&mtx);
        }
        sleep_ms(2);
    }
}