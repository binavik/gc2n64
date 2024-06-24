#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "joybus.pio.h"
#include "common.h"
#include "gamecube_controller.h"
#if DEBUG
#include <stdio.h>
#include <string.h>
#endif

const uint8_t disconnect_probe_command[1] = {0x00};
const uint8_t origin_probe_command[1] = {0x41};
const uint8_t poll_command_no_rumble[3] = {0x40, 0x03, 0x00};
const uint8_t poll_command_rumble[3] = {0x40, 0x03, 0x01};

static uint8_t gc_zero[2];
static uint8_t disconnect_timer = 0;
State controller_state;

void __time_critical_func(startGC)(uint32_t* gc_status, uint8_t* n64_status, bool &read){
#if DEBUG
    fprintf(stderr, "entered startGC\n");
    gc_status[0] = 0x00;
    gc_status[1] = 0x00;
#endif
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &joybus_program);
    pio_sm_config config;
    joybus_program_init(pio, offset + joybus_offset_joybusout, GC_PIN, config);
    //stabalize
    sleep_ms(100);
    fprintf(stderr, "finished setting up pio\n");
    controller_state = DISCONNECTED;
    while(true){
        fprintf(stderr, "current controller state: %d\n", controller_state);
        //send data to controller
        pio_sm_clear_fifos(pio, 0);
        pio_sm_set_enabled(pio, 0, false);
        switch(controller_state)
        {
            default:
            case DISCONNECTED:
            {
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_set(pio_y, 0));
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_jmp(offset));
                pio_sm_put_blocking(pio, 0, ALIGNED_JOYBUS_8(0x00));
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_set(pio_y, 7));
                break;
            }
            case ZERO:
            {
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_set(pio_y, 0));
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_jmp(offset));
                pio_sm_put_blocking(pio, 0, ALIGNED_JOYBUS_8(0x41));
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_set(pio_y, 7));
                break;
            }
            case CONNECTED:
            {
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_jmp(offset + joybus_offset_joybusout));
                pio_sm_put_blocking(pio, 0, ALIGNED_JOYBUS_8(0x40));
                pio_sm_put_blocking(pio, 0, ALIGNED_JOYBUS_8(0x03));
                pio_sm_put_blocking(pio, 0, ALIGNED_JOYBUS_8(0x00));
                break;
            }
            break;
        }
        pio_sm_set_enabled(pio, 0, true);
        sleep_us(500);
        switch(controller_state)
        {
            default:
            case DISCONNECTED:
            {
                pio_sm_exec(pio, 0, pio_encode_push(false, false));
                if((pio_sm_get(pio, 0) >> 17) & 0x09){
                    sleep_us(100);
                    controller_state = ZERO;
                }
                break;
            }
            case ZERO:
            {
                for(int i = 0; i < 2; i++){
                    if(!pio_sm_is_rx_fifo_empty(pio, 0)){
                        gc_status[i] = pio_sm_get(pio, 0);
                    }
                    else{
                        controller_state = DISCONNECTED;
                    }
                }
                //todo:
                //figure out what bytes are left and right sticks and set zero value
                controller_state = CONNECTED;
                
                break;
            }
            case CONNECTED:
            {
                for(int i = 0; i < 2; i++){
                    if(!pio_sm_is_rx_fifo_empty(pio, 0)){
                        gc_status[i] = pio_sm_get(pio, 0);
                    }
                    else{
                        disconnect_timer += 1;
                        if(disconnect_timer > MAX_DISCONNECTS){
                            controller_state = DISCONNECTED;
                        }
                    }
                }
#if DEBUG
                fprintf(stderr, "%x %x\n", gc_status[0], gc_status[1]);
#endif
                break;
            }
            break;
        }
    }
}