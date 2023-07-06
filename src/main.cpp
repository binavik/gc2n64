#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"

#include "comms.h"

//set to 0 to disable serial output over USB
#define DEBUG 1

auto_init_mutex(gc_data_mutex);

//one core does nothing but read data from gamecube controller
void core1(){
    startGC(gc_data_mutex);
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
    //give the gamecube controller time to initialize
    sleep_ms(50);
    startN64(gc_data_mutex);
    //readGC();
    return 0;
}