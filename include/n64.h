#ifndef n64
#define n64

#define N64_PIN 2
#define N64_SM 0

//command defines
//more info on n64 protocol https://github.com/Ryzee119/n360/blob/master/N64%20Controller%20Protocol.pdf
#define RESET 0xff
#define IDENTIFY 0x00
#define POLL 0x01
#define READ_PAK 0x02
#define WRITE_PAK 0x03

void startN64(uint8_t* n64_status);

#endif