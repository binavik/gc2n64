#include "common.h"
#include "gamecube_controller.h"
#include "joybus.pio.h"

//GameCube controller expect absolute values for sticks, with 0 being left/down and 255 being right/up
//GCC still reports a zero point, we save that to use in conversion as N64 expects relative position
//The conversion is simply N64 = Current_Stick_Value - GC_Zero_Value
static union{
    uint8_t Y;
    uint8_t X;
}gc_zero;
static uint8_t disconnect_timer = 0;
static gc_n64_mapping *current_mapping;
static Mapping_State mapping_state = CHECK_MAPPING;
uint8_t mapping_hold = 0;
uint8_t disconnected[4] = {0x00, 0x00, 0x00, 0x00};

void __time_critical_func(startGC)(uint32_t* gc_status, uint8_t* n64_status, State &gc_state, uint &offset, gc_n64_mapping** mappings){
#if DEBUG
    gc_status[0] = 0x00;
    gc_status[1] = 0x00;
#endif
    current_mapping = mappings[0];
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
            case RECALIBRATE:
            {
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_set(pio_y, 0));
                pio_sm_exec_wait_blocking(pio, 0, pio_encode_jmp(offset));
                pio_sm_put_blocking(pio, 0, ALIGNED_JOYBUS_8(0x42));
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
                memcpy(n64_status, disconnected, 4);
                break;
            }
            case ZERO:
            case RECALIBRATE:
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
                    (bytes[current_mapping -> A.byte] & (0x01 << current_mapping -> A.bit) ? 0x80 : 0x00) |
                    (bytes[current_mapping -> B.byte] & (0x01 << current_mapping -> B.bit) ? 0x40 : 0x00) |
                    (bytes[current_mapping -> Z.byte] & (0x01 << current_mapping -> Z.bit) ? 0x20 : 0x00) |
                    (bytes[current_mapping -> Start.byte] & (0x01 << current_mapping -> Start.bit) ? 0x10 : 0x00) |
                    (bytes[current_mapping -> Dup.byte] & (0x01 << current_mapping -> Dup.bit) ? 0x08 : 0x00) |
                    (bytes[current_mapping -> Ddown.byte] & (0x01 << current_mapping -> Ddown.bit) ? 0x04 : 0x00) |
                    (bytes[current_mapping -> Dleft.byte] & (0x01 << current_mapping -> Dleft.bit) ? 0x02 : 0x00) |
                    (bytes[current_mapping -> Dright.byte] & (0x01 << current_mapping -> Dright.bit) ? 0x01 : 0x00)
                    ;
                n64_status[1] = 0x00 |
                    (bytes[current_mapping -> L.byte] & (0x01 << current_mapping -> L.bit) ? 0x20 : 0x00) |
                    (bytes[current_mapping -> R.byte] & (0x01 << current_mapping -> R.bit) ? 0x10 : 0x00) |
                    (bytes[6] > MAX_POSITIVE ? 0x08 : 0x00) |   //Cup
                    (bytes[6] < MAX_NEGATIVE ? 0x04 : 0x00) |   //Cdown
                    (bytes[7] < MAX_NEGATIVE ? 0x02 : 0x00) |   //Cleft
                    (bytes[7] > MAX_POSITIVE ? 0x01 : 0x00)     //Cright
                    ;
                if(current_mapping -> Cup.byte > 0){
                    n64_status[1] |= bytes[current_mapping -> Cup.byte] & (0x01 << current_mapping -> Cup.bit) ? 0x08 : 0x00;
                }
                if(current_mapping -> Cdown.byte > 0){
                    n64_status[1] |= bytes[current_mapping -> Cdown.byte] & (0x01 << current_mapping -> Cdown.bit) ? 0x04 : 0x00;
                }
                if(current_mapping -> Cleft.byte > 0){
                    n64_status[1] |= bytes[current_mapping -> Cleft.byte] & (0x01 << current_mapping -> Cleft.bit) ? 0x02 : 0x00;
                }
                if(current_mapping -> Cright.byte > 0){
                    n64_status[1] |= bytes[current_mapping -> Cright.byte] & (0x01 << current_mapping -> Cright.bit) ? 0x01 : 0x00;
                }
                n64_status[2] = (uint8_t)(bytes[1] - gc_zero.X);
                n64_status[3] = (uint8_t)(bytes[0] - gc_zero.Y);

                if((n64_status[1] & 0x20) && (n64_status[1] & 0x10) && (n64_status[0] & 0x10)){
                    n64_status[0] &= 0xef;
                    n64_status[1] |= 0x80;
                    gc_state = RECALIBRATE;
                }

                //check for mapping change
                switch (mapping_state){
                    default:
                    case CHECK_MAPPING:
                        {
                            if(((bytes[2] & 0x60) | (bytes[3] & 0x08)) == 0x68){
                                mapping_state = HOLD_NEW_MAPPING;
                            }
                            break;
                        }
                    
                    case HOLD_NEW_MAPPING:
                        {    
                            mapping_hold = (bytes[2] & 0x0f);
                            if(((bytes[2] & 0x60) | (bytes[3] & 0x08)) != 0x68){
                                mapping_state = CHANGE_MAPPING;
                            }
                            break;
                        }    
                    case CHANGE_MAPPING:
                        {
                            current_mapping = mappings[mapping_hold];
                            mapping_state = CHECK_MAPPING;
                            break;
                        }
                    break;
                }
#if DEBUG
                //fprintf(stderr, "%x %x %x %x ", n64_status[0], n64_status[1], n64_status[2], n64_status[3]);
                //fprintf(stderr, "%x %x %x\n", current_mapping, &mappings[mapping_hold], mapping_hold);
#endif
                break;
            }
            break;
        }
    }
}