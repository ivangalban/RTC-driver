#include <rtc.h>
#include <io.h>


void NMI_enable(){
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) & 0x7F);
}


void NMI_disable(){
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) | 0x80);
}


void rtc_init(){

}
