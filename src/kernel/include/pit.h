#include <typedef.h>
#include <interrupts.h>

//I/O port     Usage
#define PIT_CHANNEL0_DATA_PORT    0x40         //Channel 0 data port (read/write)
#define PIT_CHANNEL1_DATA_PORT    0x41         //Channel 1 data port (read/write)
#define PIT_CHANNEL2_DATA_PORT    0x42         //Channel 2 data port (read/write)
#define PIT_CMD_REG_DATA_PORT     0x43         //Mode/Command register (write only, a read is ignored)


/*
Bits         Usage
 6 and 7      Select channel :
                 0 0 = Channel 0
                 0 1 = Channel 1
                 1 0 = Channel 2
                 1 1 = Read-back command (8254 only)
 4 and 5      Access mode :
                 0 0 = Latch count value command
                 0 1 = Access mode: lobyte only
                 1 0 = Access mode: hibyte only
                 1 1 = Access mode: lobyte/hibyte
 1 to 3       Operating mode :
                 0 0 0 = Mode 0 (interrupt on terminal count)
                 0 0 1 = Mode 1 (hardware re-triggerable one-shot)
                 0 1 0 = Mode 2 (rate generator)
                 0 1 1 = Mode 3 (square wave generator)
                 1 0 0 = Mode 4 (software triggered strobe)
                 1 0 1 = Mode 5 (hardware triggered strobe)
                 1 1 0 = Mode 2 (rate generator, same as 010b)
                 1 1 1 = Mode 3 (square wave generator, same as 011b)
 0            BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD*/

#define PIT_BINARYMODE		    0b0
#define PIT_BCDMODE		        0b1
#define PIT_MODE0		        0b0000
#define PIT_MODE1		        0b0010
#define PIT_MODE2		        0b0100
#define PIT_MODE3		        0b0110
#define PIT_MODE4		        0b1000
#define PIT_MODE5		        0b1010
#define PIT_MODE6		        0b1100
#define PIT_MODE7		        0b1110
#define PIT_LATCH_COUNT	        0b000000
#define PIT_ONLY_LOBYTE	        0b010000
#define PIT_ONLY_HIBYTE	        0b100000
#define PIT_LOBYTE_HIBYTE       0b110000
#define PIT_CHANNEL0	        0b00000000
#define PIT_CHANNEL1	        0b01000000
#define PIT_CHANNEL2            0b10000000
#define PIT_CMD_READ_BACK       0b11000000


#define PIT_OSCILATOR_FREQUENCY	1193182
#define PIT_OUTPUT_FREQUENCY	100
#define PIT_RELOAD_VALUE	PIT_OSCILATOR_FREQUENCY / PIT_OUTPUT_FREQUENCY