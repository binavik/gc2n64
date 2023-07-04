#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "joybus.pio.h"

#include "crc_table.h"
#include "comms.h"

//set to 0 to disable serial output over USB
#define DEBUG 1

auto_init_mutex(gc_data_mutex);

//global variable, 8 bytes from controller
//byte 0: bits 0, 0, 0, start, y, x, b, a
//byte 1: bits 1, L, R, Z, Dup, Ddown, Dright, Dleft
//byte 2: stick_x
//byte 3: stick_y
//byte 4: cstick_x
//byte 5: cstick_y
//byte 6: left_trigger
//byte 7: right_trigger
static uint8_t gc_status[8];
//GC gives absolute position of stick, n64 wants relative position
//GC still reports zero point so we can use it for the N64, just needs to be saved first
static uint8_t gc_zero[2];

// max recv is 1+2+32 bytes + 1 bit
static uint8_t n64_raw_dump[281];
// deoes not include command byte, so:
static uint8_t n64_command;

//data we'll be sending
static uint8_t n64_buffer[33];

// zero point for GC stick
static uint8_t zero_x;
static uint8_t zero_y;

static unsigned int counter; 

void core1(){
    startGC(gc_status, gc_zero,gc_data_mutex);
}

#if DEBUG
void print_gc(){
    uint8_t data[8];
    mutex_enter_blocking(&gc_data_mutex);
    //fprintf(stderr, "in core 0\n");
    memcpy(data, gc_status, 8);
    mutex_exit(&gc_data_mutex);
    fprintf(stderr, "%x %x %x %x %x %x %x %x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    //mutex_exit(&gc_data_mutex);
    //sleep_ms(2);
}

void core1_print_loop(){
    while(true){
    uint8_t data[8];
    mutex_enter_blocking(&gc_data_mutex);
    //fprintf(stderr, "in core 0\n");
    memcpy(data, gc_status, 8);
    memset(gc_status, 0, 8);
    //controller disconnected, try loading it back up
    if(data[1] == 0){
        watchdog_enable(100, 0);
        while(1){}
    }
    else{
        fprintf(stderr, "%x %x %x %x %x %x %x %x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
        mutex_exit(&gc_data_mutex);
    }
    sleep_ms(2);
    }
}
#endif

void __time_critical_func(convertToPio)(const uint8_t* command, const int len, uint32_t* result, int& resultLen){
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

void __time_critical_func(readGC)(){  
    printf("pio setup\n");  
    PIO pio = pio0;
    pio_gpio_init(pio, GC_PIN);
    uint offset = pio_add_program(pio, &joybus_program);
    printf("pio setup done\nconfig setup\n");
    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, GC_PIN);
    sm_config_set_out_pins(&config, GC_PIN, 1);
    sm_config_set_set_pins(&config, GC_PIN, 1);
    sm_config_set_clkdiv(&config, 5);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);
    printf("config setup done\ninitial init\n");
    pio_sm_init(pio, 0, offset, &config);
    pio_sm_set_enabled(pio, 0, true);

    uint8_t init[3] = {0x40, 0x03, 0x00};
    uint32_t result[3];
    int resultLen;
    convertToPio(init, 3, result, resultLen);
    printf("ResultLen: %d\n", resultLen);
    printf("hex: %X\n", result[0]);
    printf("first blocking put\n");
    unsigned int itr = 0;
    while(true){
        //fprintf(stderr, "iteration %d\n", itr++);
        pio_sm_set_enabled(pio, 0, false);
        pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
        pio_sm_set_enabled(pio, 0, true);
        for(int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
        for(int i = 0; i<8; i++) {
            gc_status[i] = pio_sm_get_blocking(pio, 0);
        }
        //sleep_ms(16);
        //fprintf(stderr, "\033c");
        fprintf(stderr, "0:%x 1:%x 2:%x 3:%x 4:%x 5:%x 6:%x 7:%x\n", gc_status[0], gc_status[1], gc_status[2], gc_status[3], gc_status[4], gc_status[5], gc_status[6], gc_status[7]);
        //sleep_ms(32);
    }
}

int main() {
//init USB serial communication, disable 
#if DEBUG
    stdio_init_all();
#endif
    gpio_init(GC_PIN);
    gpio_set_dir(GC_PIN, GPIO_IN);
    gpio_init(N64_PIN);
    gpio_set_dir(N64_PIN, GPIO_IN);

    //getchar();
    //startGC(gc_status, gc_data_mutex);
    multicore_launch_core1(core1);
    sleep_ms(50);
    counter = 0;
    core1_print_loop();
    //readGC();
    return 0;
}