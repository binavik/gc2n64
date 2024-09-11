#include "common.h"
#include "n64.h"
#include "joybusN64.pio.h"
#include "hardware/clocks.h"

uint8_t command;
uint8_t* _n64_status;
uint8_t local_status[4];
uint32_t out_data[1024];
PIO pio;
uint _offset;
pio_sm_config config;


void __time_critical_func(rx_handler)(){
    if(!pio_interrupt_get(pio, 0)) return;

    memcpy(local_status, _n64_status, 4);
    int c = 40;
    while(c--) asm("nop");
    command = pio_sm_get(pio, N64_SM);

    switch (command){
        case IDENTIFY:
        case RESET:
            {
                c = 20;
                while(c--) asm("nop");
                joybusN64_set_in(false, pio, N64_SM, _offset, &config, N64_PIN);
                pio_sm_put_blocking(pio, N64_SM, ALIGNED_JOYBUS_8(0x05));
                pio_sm_put_blocking(pio, N64_SM, ALIGNED_JOYBUS_8(0x00));
                pio_sm_put_blocking(pio, N64_SM, ALIGNED_JOYBUS_8(0x02));
                break;
            }
        case POLL:
            {
                c = 40;
                while(c--) asm("nop");
                joybusN64_set_in(false, pio, N64_SM, _offset, &config, N64_PIN);
                for(int i = 0; i < 4; i++){
                    pio_sm_put_blocking(pio, N64_SM, ALIGNED_JOYBUS_8(local_status[i]));
                }
            }
        default:
            break;
    }
    pio_interrupt_clear(pio, 0);
}

void __time_critical_func(tx_handler)(){
    if(!pio_interrupt_get(pio, 1)) return;
    pio_interrupt_clear(pio, 1);
    joybusN64_set_in(true, pio, N64_SM, _offset, &config, N64_PIN);
}

void startN64(uint8_t* n64_status){
    //init stuff
    _n64_status = n64_status;
    pio = pio1;
    _offset = pio_add_program(pio, &joybusN64_program);
    
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);

    irq_set_exclusive_handler(PIO1_IRQ_0, rx_handler);
    irq_set_exclusive_handler(PIO1_IRQ_1, tx_handler);
    
    joybusN64_program_init(pio, N64_SM, _offset, N64_PIN, &config);
    irq_set_enabled(PIO1_IRQ_0, true);
    irq_set_enabled(PIO1_IRQ_1, true);
    //everything other than init is handled via interrupts
    while(true){}
}