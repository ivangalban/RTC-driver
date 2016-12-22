#include <time.h>
#include <rtc.h>
#include <fb.h>

#define CURRENT_YEAR        2016

void time_show(struct tm *t) {
	fb_printf("Date: %dd/%dd/%dd\n", t->day, t->month, t->year);
	fb_printf("Time: %dd:%dd:%dd\n", t->hours, t->minutes, t->seconds);
	fb_printf("\n");
}

u8 century;

void time_load(struct tm *t, char *buf) {

	// Make sure an update isn't in progress
	while (get_update_in_progress_flag());

	fdrtc->f_ops.read(fdrtc, buf, REGISTER_COUNT);

	t->seconds = buf[0];
	t->minutes = buf[1];
	t->hours   = buf[2];
	t->day     = buf[3];
	t->month   = buf[4];
	t->year    = buf[5];
	century = get_RTC_register(REG_CENTURY);
}


//Obtiene la fecha y la hora actuales.
void time_get(struct tm *t) {
	
	char buf[REGISTER_COUNT];
	u8 last_second;
	u8 last_minute;
	u8 last_hour;
	u8 last_day;
	u8 last_month;
	u8 last_year;
	u8 registerB;

	// Note: This uses the "read registers until you get the same values twice in a row" technique
	//       to avoid getting dodgy/inconsistent values due to RTC updates

	time_load(t, buf);

	do {
		last_second = t->seconds;
		last_minute = t->minutes;
		last_hour = t->hours;
		last_day = t->day;
		last_month = t->month;
		last_year = t->year;

		time_load(t, buf);

	} while( (last_second != t->seconds) || (last_minute != t->minutes) || (last_hour != t->hours) ||
	   (last_day != t->day) || (last_month != t->month) || (last_year != t->year) );

	registerB = buf[6];

	// Convert BCD to binary values if necessary
	if (!(registerB & BINARY_MODE)) {
	    t->seconds = (t->seconds & 0x0F) + ((t->seconds / 16) * 10);
	    t->minutes = (t->minutes & 0x0F) + ((t->minutes / 16) * 10);
	    t->hours = ( (t->hours & 0x0F) + (((t->hours & 0x70) / 16) * 10) ) | (t->hours & 0x80);
	    t->day = (t->day & 0x0F) + ((t->day / 16) * 10);
	    t->month = (t->month & 0x0F) + ((t->month / 16) * 10);
	    t->year = (t->year & 0x0F) + ((t->year / 16) * 10);
	    century = (century & 0x0F) + ((century / 16) * 10);
	}

	// Convert 12 hour clock to 24 hour clock if necessary

	if (!(registerB & FORMAT_24HOURS) && (t->hours & 0x80))
	    t->hours = ((t->hours & 0x7F) + 12) % 24;

	// Calculate the full (4-digit) year
	t->year += century * 100;
}


//Establece la fecha y la hora actuales.
void time_set(struct tm *t) {
	char buf[] = {t-> seconds, t-> minutes, t-> hours, 
				  t-> day, t-> month, t-> year % 100};

	fdrtc->f_ops.write(fdrtc, buf, REGISTER_COUNT); 
}

u64 get_seconds(struct tm *t) {
	return t->seconds + t->minutes*60 + t->hours*60*60 + 
			t->day*24*60*60 + t->month*30*24*60*60 + 
			t->year*365*24*60*60;
}

//Detiene la ejecución por algunos segundos.
void time_sleep(int s) {
	struct tm t;
	time_get(&t);
	u64 seconds_start = get_seconds(&t);
	u64 seconds_end	  = seconds_start + s;

	do{
		time_get(&t);
	}while(get_seconds(&t) < seconds_end);
}