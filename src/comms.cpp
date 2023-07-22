#include <stdio.h>
#include <string.h>
#include <pico/mutex.h>
#include "hardware/watchdog.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "joybus.pio.h"

#include "comms.h"
#include "crc_table.h"

static uint8_t gc_zero[2];

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

void __time_critical_func(startGC)(uint8_t* gc_status, uint8_t* n64_status, bool &read){
    PIO pio = pio0;
    pio_gpio_init(pio, GC_PIN);
    uint offset = pio_add_program(pio, &joybus_program);

    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, GC_PIN);
    sm_config_set_out_pins(&config, GC_PIN, 1);
    sm_config_set_set_pins(&config, GC_PIN, 1);
    float freq = 1000000;
    float clockDiv = clock_get_hz(clk_sys) / (25 * freq);
    sm_config_set_clkdiv(&config, clockDiv);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);
    pio_sm_set_enabled(pio, 0, true);

    //uint8_t gc_read_command[3] = {0x40, 0x03, 0x00};
    //uint32_t result[3]; 
    uint32_t result[2] = {0xfaaaaaae, 0x3aaaa}; 
    //int resultLen;
    int resultLen = 2;
    //convertToPio(gc_read_command, 3, result, resultLen);

    //init controller and get zero point, do this once outside of read loop
    pio_sm_set_enabled(pio, 0, false);
    pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
    pio_sm_set_enabled(pio, 0, true);
    watchdog_enable(WATCHDOG_DELAY_MS, 0);

    for(int i = 0; i < resultLen; i++){
        pio_sm_put_blocking(pio, 0, result[i]);
    }
    for(int i = 0; i < GC_ARR_SIZE; i++){
        gc_status[i] = pio_sm_get_blocking(pio, 0);
    }
    gc_zero[0] = -gc_status[2];      //zero x
    gc_zero[1] = -gc_status[3];      //zero y
    read = false;
    while(true){
        while(!read){
            watchdog_update();
        }

        pio_sm_set_enabled(pio, 0, false);
        pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
        pio_sm_set_enabled(pio, 0, true);

        for(int i = 0; i < resultLen; i++){
            pio_sm_put_blocking(pio, 0, result[i]);
        }
        for(int i = 0; i < GC_ARR_SIZE; i++){
            gc_status[i] = pio_sm_get_blocking(pio, 0);
        }
    
        memset(n64_status, 0, 4);

        //n64_status[N64_A_BYTE] |= ((gc_status[GC_A_BYTE] >> GC_A_BIT) & 0x01) << N64_A_BIT;
        n64_status[N64_A_BYTE] |= (gc_status[GC_A_BYTE] & (0x01 << GC_A_BIT)) ? 0x80 : 0x00;
       // n64_status[N64_B_BYTE] |= ((gc_status[GC_B_BYTE] >> GC_B_BIT) & 0x01) << N64_B_BIT;
        n64_status[N64_B_BYTE] |= (gc_status[GC_B_BYTE] & (0x01 << GC_B_BIT)) ? 0x40 : 0x00;
        //n64_status[N64_Z_BYTE] |= ((gc_status[GC_Z_BYTE] >> GC_Z_BIT) & 0x01) << N64_Z_BIT;
        n64_status[N64_Z_BYTE] |= (gc_status[GC_Z_BYTE] & (0x01 << GC_Z_BIT)) ? 0x20 : 0x00;
        //n64_status[N64_START_BYTE] |= ((gc_status[GC_START_BYTE] >> GC_START_BIT) & 0x01) << N64_START_BIT;
        n64_status[N64_START_BYTE] |= (gc_status[GC_START_BYTE] & (0x01 << GC_START_BIT)) ? 0x10 : 0x00;
        //n64_status[N64_D_UP_BYTE] |= ((gc_status[GC_D_UP_BYTE] >> GC_D_UP_BIT) & 0x01) << N64_D_UP_BIT;
        n64_status[N64_D_UP_BYTE] |= (gc_status[GC_D_UP_BYTE] & (0x01 << GC_D_UP_BIT)) ? 0x08 : 0x00;
        //n64_status[N64_D_DOWN_BYTE] |= ((gc_status[GC_D_DOWN_BYTE] >> GC_D_DOWN_BIT) & 0x01) << N64_D_DOWN_BIT;
        n64_status[N64_D_DOWN_BYTE] |= (gc_status[GC_D_DOWN_BYTE] & (0x01 << GC_D_DOWN_BIT)) ? 0x04 : 0x00;
        //n64_status[N64_D_LEFT_BYTE] |= ((gc_status[GC_D_LEFT_BYTE] >> GC_D_LEFT_BIT) & 0x01) << N64_D_LEFT_BIT;
        n64_status[N64_D_LEFT_BYTE] |= (gc_status[GC_D_LEFT_BYTE] & (0x01 << GC_D_LEFT_BIT)) ? 0x02 : 0x00;
        //n64_status[N64_D_RIGHT_BYTE] |= ((gc_status[GC_D_RIGHT_BYTE] >> GC_D_RIGHT_BIT) & 0x01) << N64_D_RIGHT_BIT;
        n64_status[N64_D_RIGHT_BYTE] |= (gc_status[GC_D_RIGHT_BYTE] & (0x01 << GC_D_RIGHT_BIT)) ? 0x01 : 0x00;
        //n64_status[N64_L_BYTE] |= ((gc_status[GC_L_BYTE] >> GC_L_BIT) & 0x01) << N64_L_BIT;
        n64_status[N64_L_BYTE] |= (gc_status[GC_L_BYTE] & (0x01 << GC_L_BIT)) ? 0x20 : 0x00;
        //n64_status[N64_R_BYTE] |= ((gc_status[GC_R_BYTE] >> GC_R_BIT) & 0x01) << N64_R_BIT;
        n64_status[N64_R_BYTE] |= (gc_status[GC_R_BYTE] & (0x01 << GC_R_BIT)) ? 0x10 : 0x00;
        n64_status[N64_C_UP_BYTE] |= (gc_status[5] > MAX_POSITIVE) ? 0x08 : 0x00;
        n64_status[N64_C_DOWN_BYTE] |= (gc_status[5] < MAX_NEGATIVE) ? 0x04 : 0x00;
        n64_status[N64_C_LEFT_BYTE] |= (gc_status[4] < MAX_NEGATIVE) ? 0x02 : 0x00;
        n64_status[N64_C_RIGHT_BYTE] |= (gc_status[4] > MAX_POSITIVE) ? 0x01 : 0x00;
        n64_status[2] = gc_zero[0] + gc_status[2];
        n64_status[3] = gc_zero[1] + gc_status[3];
        n64_status[1] |= (gc_status[6] > MAX_TRIGGER) ? 0x20 : 0x00;
        n64_status[1] |= (gc_status[7] > MAX_TRIGGER) ? 0x10 : 0x00;

        if((n64_status[1] & 0x20) && (n64_status[1] & 0x10) && (n64_status[0] & 0x10)){
            n64_status[0] &= 0xEF;
            n64_status[1] |= 0x80;
            gc_zero[0] = -gc_status[2];      //zero x
            gc_zero[1] = -gc_status[3];      //zero y
        }

        watchdog_update();
        read = false;
    }
}

void __time_critical_func(startN64)(uint8_t* gc_status, uint8_t* n64_status, bool &read){
    PIO pio = pio1;
    pio_gpio_init(pio, N64_PIN);
    uint offset = pio_add_program(pio, &joybus_program);

    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, N64_PIN);
    sm_config_set_out_pins(&config, N64_PIN, 1);
    sm_config_set_set_pins(&config, N64_PIN, 1);
    float freq = 1000000;
    float clockDiv = clock_get_hz(clk_sys) / (25 * freq);
    sm_config_set_clkdiv(&config, clockDiv);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);

    //uint8_t n64_info_command[3] = {0x05, 0x00, 0x02};
    //uint32_t n64_info_response[8];
    uint32_t n64_info_response[2] = {0xaaaaeeaa, 0x3baaa};
    //int info_send_len;
    int info_send_len = 2;
    //convertToPio(n64_info_command, 3, n64_info_response, info_send_len);

    read = true;
    pio_sm_init(pio, 0, offset+joybus_offset_inmode, &config);
    pio_sm_set_enabled(pio, 0, true);
    while(true){
        uint8_t command = pio_sm_get_blocking(pio, 0);
#if DEBUG
        fprintf(stderr, "command: %x\n", command);
#endif
        if(command == 0xff || command == 0x00){
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);
            while(read){};
            sleep_us(4);

            for(int i = 0; i < info_send_len; i++){
                pio_sm_put_blocking(pio, 0, n64_info_response[i]);
            }
        }
        else if(command == 0x01){
            uint32_t result[6];
            int result_len;
            convertToPio(n64_status, 4, result, result_len);
            while(read){}
            sleep_us(4);

            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for(int i = 0; i < result_len; i++){
                pio_sm_put_blocking(pio, 0, result[i]);
            }    
        }
        else{
#if DEBUG
            fprintf(stderr, "wtf am I even doing here?\n");
#endif
            pio_sm_set_enabled(pio, 0, false);
            sleep_us(100);
            pio_sm_init(pio, 0, offset+joybus_offset_inmode, &config);
            pio_sm_set_enabled(pio, 0, true);
        }
        read = true;
    }
}
