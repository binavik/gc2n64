#include <stdio.h>
#include <string.h>
#include <pico/mutex.h>
#include "hardware/watchdog.h"
#include "hardware/pio.h"
#include "joybus.pio.h"

#include "comms.h"
#include "crc_table.h"

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
static uint8_t n64_status[4];
//N64 expects relative, or signed, values for the sticks.

void __time_critical_func(convertToPio)(const uint8_t* command, const int len, uint32_t* result, int& resultLen){
    // PIO Shifts to the right by default
    // In: pushes batches of 8 shifted left, i.e we get [0x40, 0x03, rumble (the end bit is never pushed)]
    //      commands from N64 console or results from gamecube controller
    // Out: We push commands for a right shift with an enable pin, ie 5 (101) would be 0b11'10'11
    //      commands to the gamecube controller or controller status to N64 console
    // So in doesn't need post processing but out does
    if (len == 0) {
        resultLen = 0;
        return;
    }
    resultLen = len/2 + 1;
    int i;
    //is memset faster or more efficient? 
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

void __time_critical_func(startGC)(mutex_t mtx, uint8_t DEBUG){
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
    convertToPio(gc_read_command, 3, result, resultLen);

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

void __time_critical_func(startN64)(mutex_t mtx, uint8_t DEBUG){
    PIO pio = pio1;
    pio_gpio_init(pio, N64_PIN);
    uint offset = pio_add_program(pio, &joybus_program);

    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, N64_PIN);
    sm_config_set_out_pins(&config, N64_PIN, 1);
    sm_config_set_set_pins(&config, N64_PIN, 1);
    sm_config_set_clkdiv(&config, 5);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);

    //command to send as a response to 0x00 and 0xFF
    //0x0500 sent by all controllers, 3rd byte is 0x01 if a controller pack is inserted 
    //or 0x02 if there is no controller pack inserted
    //or 0x04 if last crc had an error
    uint8_t n64_info_command[3] = {0x05, 0x00, 0x02};
    uint32_t n64_info_response[4];
    int info_send_len;
    convertToPio(n64_info_command, 3, n64_info_response, info_send_len);

    uint8_t command;
    uint8_t data[8];

    pio_sm_init(pio, 0, offset+joybus_offset_inmode, &config);
    pio_sm_set_enabled(pio, 0, true);

    while(true){
        mutex_enter_blocking(&mtx);
        memcpy(data, gc_status, 8);
        memset(gc_status, 0, 8);
        mutex_exit(&mtx);
        memset(n64_status, 0, 4);

        n64_status[N64_A_BYTE] |= ((data[GC_A_BYTE] >> GC_A_BIT) & 0x01) << N64_A_BIT;
        n64_status[N64_B_BYTE] |= ((data[GC_B_BYTE] >> GC_B_BIT) & 0x01) << N64_B_BIT;
        n64_status[N64_Z_BYTE] |= ((data[GC_Z_BYTE] >> GC_Z_BIT) & 0x01) << N64_Z_BIT;
        n64_status[N64_START_BYTE] |= ((data[GC_START_BYTE] >> GC_START_BIT) & 0x01) << N64_START_BIT;
        n64_status[N64_D_UP_BYTE] |= ((data[GC_D_UP_BYTE] >> GC_D_UP_BIT) & 0x01) << N64_D_UP_BIT;
        n64_status[N64_D_DOWN_BYTE] |= ((data[GC_D_DOWN_BYTE] >> GC_D_DOWN_BIT) & 0x01) << N64_D_DOWN_BIT;
        n64_status[N64_D_LEFT_BYTE] |= ((data[GC_D_LEFT_BYTE] >> GC_D_LEFT_BIT) & 0x01) << N64_D_LEFT_BIT;
        n64_status[N64_D_RIGHT_BYTE] |= ((data[GC_D_RIGHT_BYTE] >> GC_D_RIGHT_BIT) & 0x01) << N64_D_RIGHT_BIT;
        n64_status[N64_L_BYTE] |= ((data[GC_L_BYTE] >> GC_L_BIT) & 0x01) << N64_L_BIT;
        n64_status[N64_R_BYTE] |= ((data[GC_R_BYTE] >> GC_R_BIT) & 0x01) << N64_R_BIT;
        n64_status[N64_C_UP_BYTE] |= ((data[5] > MAX_POSITIVE) ? 1 : 0) << N64_C_UP_BIT;
        n64_status[N64_C_DOWN_BYTE] |= ((data[5] < MAX_NEGATIVE) ? 1 : 0) << N64_C_DOWN_BIT;
        n64_status[N64_C_LEFT_BYTE] |= ((data[4] < MAX_NEGATIVE) ? 1 : 0) << N64_C_LEFT_BIT;
        n64_status[N64_C_RIGHT_BYTE] |= ((data[4] > MAX_POSITIVE) ? 1 : 0) << N64_C_RIGHT_BIT;
        n64_status[2] = -gc_zero[0] + data[2];
        n64_status[3] = -gc_zero[1] + data[3];

        //byte 1 always has the MSB set, so if 0 then the
        //controller disconnected, reset the pico to load it back
        if(data[1] == 0){
            watchdog_enable(WATCHDOG_DELAY_MS, 0);
            while(1){}
        }
        else{
            //fprintf(stderr, "%x %x %x %x\n", n64_status[0], n64_status[1], n64_status[2], n64_status[3]);
            //fprintf(stderr, "%x %x %x %x %x %x %x %x\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
            //fprintf(stderr, "%d %d %d %d\n", gc_zero[0], n64_status[2], gc_zero[1], n64_status[3]);
        }
        //fprintf(stderr, "command: ");
        command = pio_sm_get_blocking(pio, 0);
        //fprintf(stderr, "%x\n", command);
        switch (command)
        {
        case 0xFF:
        case 0x00:
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for(int i = 0; i < info_send_len; i++){
                pio_sm_put_blocking(pio, 0, n64_info_response[i]);
            }
            break;
        
        case 0x01:
            uint32_t result[6];
            int result_len;
            convertToPio(n64_status, 4, result, result_len);

            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for(int i = 0; i < result_len; i++){
                pio_sm_put_blocking(pio, 0, result[i]);
            }
            //this "delay" here is enough to get the controller functioning in the Everdrive menu and controller tester
            //games don't see a controller connected, need to try an external pullup resistor or increase the delay
            for(int i = 0; i < result_len; i++){
                fprintf(stderr, "%x ", result[i]);
            }
            fprintf(stderr, "\n");

            break;

        default:
            pio_sm_set_enabled(pio, 0, false);
            sleep_us(100);
            pio_sm_init(pio, 0, offset+joybus_offset_inmode, &config);
            pio_sm_set_enabled(pio, 0, true);
            break;
        }

    }
}