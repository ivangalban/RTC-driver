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


u8 rtc_get_seconds(){
	NMI_disable();
	outb(CMOS_ADDRESS, REG_SECONDS);
	return inb(CMOS_DATA);
}


u8 rtc_get_minutes(){
	NMI_disable();
	outb(CMOS_ADDRESS, REG_MINUTES);
	return inb(CMOS_DATA);
}


u8 rtc_get_hours(){
	NMI_disable();
	outb(CMOS_ADDRESS, REG_HOURS);
	return inb(CMOS_DATA);
}


u8 rtc_get_day(){
	NMI_disable();
	outb(CMOS_ADDRESS, REG_DAY);
	return inb(CMOS_DATA);
}


u8 rtc_get_month(){
	NMI_disable();
	outb(CMOS_ADDRESS, REG_MONTH);
	return inb(CMOS_DATA);
}


u32 rtc_get_year(){
	NMI_disable();
	outd(CMOS_ADDRESS, REG_YEAR);
	return ind(CMOS_DATA);
}