#include <rtc.h>
#include <io.h>
#include <hw.h>

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
      return inb(CMOS_DATA);
}

void set_RTC_register(u8 reg_addres, u8 data) {
	hw_cli();
	outb(CMOS_ADDRESS, reg_addres);
	outb(CMOS_DATA, data);
	hw_sti();
}


int get_update_in_progress_flag() {
      outb(CMOS_ADDRESS, REGA_STATUS);
      return (inb(CMOS_ADDRESS) & 0x80);
}