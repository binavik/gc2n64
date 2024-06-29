#include "common.h"
#include "gamecube_controller.h"

//GameCube controller expect absolute values for sticks, with 0 being left/down and 255 being right/up
//GCC still reports a zero point, we save that to use in conversion as N64 expects relative position
//The conversion is simply N64 = Current_Stick_Value - GC_Zero_Value
static union{
    uint8_t Y;
    uint8_t X;
}gc_zero;
static uint8_t disconnect_timer = 0;

void __time_critical_func(startGC)(uint32_t* gc_status, uint8_t* n64_status, bool &read, State &gc_state, uint &offset){
#if DEBUG
    gc_status[0] = 0x00;
    gc_status[1] = 0x00;
#endif
    uint8_t *bytes = (uint8_t*)gc_status;
    PIO pio = pio0;
    pio_sm_config config;
    joybus_program_init(pio, offset + joybus_offset_joybusout, GC_PIN, config, 32, 0);
    //stabalize
    sleep_ms(100);
    gc_state = DISCONNECTED;
    while(true){
        //send data to controller
        pio_sm_clear_fifos(pio, 0);
        pio_sm_set_enabled(pio, 0, false);
        switch(gc_state)
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
        switch(gc_state)
        {
            default:
            case DISCONNECTED:
            {
                pio_sm_exec(pio, 0, pio_encode_push(false, false));
                if((pio_sm_get(pio, 0) >> 17) & 0x09){
                    sleep_us(100);
                    gc_state = ZERO;
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
                        gc_state = DISCONNECTED;
                    }
                }
                memcpy(bytes, gc_status, 2);
                gc_zero.Y = bytes[0];
                gc_zero.X = bytes[1];
                gc_state = CONNECTED;
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
                            gc_state = DISCONNECTED;
                        }
                    }
                }
                n64_status[0] =
                    ((bytes[3] & 0x01) << 7) | //A
                    ((bytes[3] & 0x02) << 5) | //B
                    ((bytes[2] & 0x10) << 1) | //Z
                    ((bytes[3] & 0x10))      | //Start
                    ((bytes[2] & 0x0c))      | //Dpad up and down
                    ((bytes[2] & 0x01) << 1) | //Dpad left
                    ((bytes[2] & 0x02) >> 1)   //Dpad right
                    ;
                n64_status[1] = 0x00 |
                    ((bytes[2] & 0x60) >> 1)                | //L and R
                    (bytes[6] > MAX_POSITIVE ? 0x08 : 0x00) | //C up
                    (bytes[6] < MAX_NEGATIVE ? 0x04 : 0x00) | //C down
                    (bytes[7] < MAX_NEGATIVE ? 0x02 : 0x00) | //C left
                    (bytes[7] > MAX_POSITIVE ? 0x01 : 0x00)   //C right
                    ;
                n64_status[2] = (uint8_t)(bytes[1] - gc_zero.X);
                n64_status[3] = (uint8_t)(bytes[0] - gc_zero.Y);

                if((n64_status[1] & 0x20) && (n64_status[1] & 0x10) && (n64_status[0] & 0x10)){
                    n64_status[0] &= 0xef;
                    n64_status[1] |= 0x80;
                    gc_zero.X = bytes[1];
                    gc_zero.Y = bytes[0];
                }
#if DEBUG
                //fprintf(stderr, "%x %x %x %x\n", n64_status[0], n64_status[1], n64_status[2], n64_status[3]);
#endif
                break;
            }
            break;
        }
    }
}