#include <time.h>
#include <rtc.h>

#define CURRENT_YEAR        2016

// Set by ACPI table parsing code if possible
int century_register = 0x00;

 

//Obtiene la fecha y la hora actuales.
void time_get(struct tm *t) {
	
	//fdrtc->;
	/*u8 century;
	u8 last_second;
	u8 last_minute;
	u8 last_hour;
	u8 last_day;
	u8 last_month;
	u8 last_year;
	u8 last_century;
	u8 registerB;
 
	// Note: This uses the "read registers until you get the same values twice in a row" technique
	//       to avoid getting dodgy/inconsistent values due to RTC updates
 	
 	// Make sure an update isn't in progress
	while (get_update_in_progress_flag());

	t->seconds = get_RTC_register(REG_SECONDS);
	t->minutes = get_RTC_register(REG_MINUTES);
	t->hours = get_RTC_register(REG_HOURS);
	t->day = get_RTC_register(REG_DAY);
	t->month = get_RTC_register(REG_MONTH);
	t->year = get_RTC_register(REG_YEAR);

	if(century_register != 0) {
	    century = get_RTC_register(century_register);
	}
 
	do {
	    last_second = t->seconds;
	    last_minute = t->minutes;
	    last_hour = t->hours;
	    last_day = t->day;
	    last_month = t->month;
	    last_year = t->year;
	    last_century = century;

	    // Make sure an update isn't in progress
	    while (get_update_in_progress_flag());

	    t->seconds = get_RTC_register(REG_SECONDS);
	    t->minutes = get_RTC_register(REG_MINUTES);
	    t->hours = get_RTC_register(REG_HOURS);
	    t->day = get_RTC_register(REG_DAY);
	    t->month = get_RTC_register(REG_MONTH);
	    t->year = get_RTC_register(REG_YEAR);

	    if(century_register != 0) {
	          century = get_RTC_register(century_register);
	    }
	} while( (last_second != t->seconds) || (last_minute != t->minutes) || (last_hour != t->hours) ||
	       (last_day != t->day) || (last_month != t->month) || (last_year != t->year) ||
	       (last_century != century) );
 
      registerB = get_RTC_register(REGB_STATUS);
 
      // Convert BCD to binary values if necessary
 
      if (!(registerB & BINARY_MODE)) {
            t->seconds = (t->seconds & 0x0F) + ((t->seconds / 16) * 10);
            t->minutes = (t->minutes & 0x0F) + ((t->minutes / 16) * 10);
            t->hours = ( (t->hours & 0x0F) + (((t->hours & 0x70) / 16) * 10) ) | (t->hours & 0x80);
            t->day = (t->day & 0x0F) + ((t->day / 16) * 10);
            t->month = (t->month & 0x0F) + ((t->month / 16) * 10);
            t->year = (t->year & 0x0F) + ((t->year / 16) * 10);
            if(century_register != 0) {
                  century = (century & 0x0F) + ((century / 16) * 10);
            }
      }
 
      // Convert 12 hour clock to 24 hour clock if necessary
 
      if (!(registerB & FORMAT_24HOURS) && (t->hours & 0x80)) {
            t->hours = ((t->hours & 0x7F) + 12) % 24;
      }
 
      // Calculate the full (4-digit) year
 
      if(century_register != 0) {
            t->year += century * 100;
      } else {
            t->year += (CURRENT_YEAR / 100) * 100;
            if(t->year < CURRENT_YEAR) t->year += 100;
      }*/

}


//Establece la fecha y la hora actuales.
void time_set(struct tm *t) {
	char buf[] = {t-> seconds, t-> minutes, t-> hours, 
				  t-> day, t-> month, t-> year % 100};

	fdrtc->f_ops.write(fdrtc, buf, 6); 
}


//Detiene la ejecución por algunos segundos.
void time_sleep(int s) {

}