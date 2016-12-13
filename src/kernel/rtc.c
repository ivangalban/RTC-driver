#include <rtc.h>
#include <io.h>

void rtc_init(){

}


u8 rtc_get_seconds(){
	outb(CMOS_ADDRESS, REG_SECONDS);
	return inb(CMOS_DATA);
}


u8 rtc_get_minutes(){
	outb(CMOS_ADDRESS, REG_MINUTES);
	return inb(CMOS_DATA);
}


u8 rtc_get_hours(){
	outb(CMOS_ADDRESS, REG_HOURS);
	return inb(CMOS_DATA);
}


u8 rtc_get_day(){
	outb(CMOS_ADDRESS, REG_DAY);
	return inb(CMOS_DATA);
}


u8 rtc_get_month(){
	outb(CMOS_ADDRESS, REG_MONTH);
	return inb(CMOS_DATA);
}


u32 rtc_get_year(){
	outd(CMOS_ADDRESS, REG_YEAR);
	return ind(CMOS_DATA);
}