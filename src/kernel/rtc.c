#include <rtc.h>
#include <io.h>


void NMI_enable() {
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) & 0x7F);
}


void NMI_disable() {
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) | 0x80);
}


void rtc_init() {

}


u8 get_RTC_register(int reg) {
      outb(CMOS_ADDRESS, reg);
      return inb(CMOS_ADDRESS);
}