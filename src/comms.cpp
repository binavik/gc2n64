#include <stdio.h>
#include "hardware/pio.h"
#include "joybus.pio.h"

//temp, delete these later

//

#include "comms.h"

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

void __time_critical_func(startGC)(uint8_t* gc_status, uint8_t* gc_zero, mutex_t mtx){
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

    uint8_t read_command[3] = {0x40, 0x03, 0x00};
    uint32_t result[3]; 
    int resultLen;
    convertToPio2(read_command, 3, result, resultLen);

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

void __time_critical_func(startN64)(uint8_t* gc_status, mutex_t mtx){

}