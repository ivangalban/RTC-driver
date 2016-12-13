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


u8 get_RTC_register(u8 reg) {
      outb(CMOS_ADDRESS, reg);
      return inb(CMOS_DATA;
}

void set_RTC_register(u8 reg_addres, u8 data) {
	outb(CMOS_ADDRESS, reg_addres);
	outb(CMOS_DATA, data);
}