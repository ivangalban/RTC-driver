#include <time.h>
#include <rtc.h>

//Obtiene la fecha y la hora actuales.
void time_get(struct tm *t) {
	
}


//Establece la fecha y la hora actuales.
void time_set(struct tm *t) {
	NMI_disable();
	
	set_RTC_register(REG_SECONDS, t->seconds);
	set_RTC_register(REG_MINUTES, t->minutes);
	set_RTC_register(REG_HOURS, t->hours);
	set_RTC_register(REG_DAY, t->day);
	set_RTC_register(REG_MONTH, t->month);
	set_RTC_register(REG_YEAR, t->year);

	NMI_enable();
}


//Detiene la ejecución por algunos segundos.
void time_sleep(int s) {

}