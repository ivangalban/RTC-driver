#include <time.h>
#include <rtc.h>

//Obtiene la fecha y la hora actuales.
void time_get(struct tm *t) {
	t-> seconds = rtc_get_seconds();
	t-> minutes = rtc_get_minutes();
	t-> hours 	= rtc_get_hours();
	t-> day 	= rtc_get_day();
	t-> month 	= rtc_get_month();
	t-> year 	= rtc_get_year();
}


//Establece la fecha y la hora actuales.
void time_set(struct tm *t) {

}


//Detiene la ejecución por algunos segundos.
void time_sleep(int s) {

}