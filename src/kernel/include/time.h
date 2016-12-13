#include <typedef.h>

struct tm {
	u8 seconds;
	u8 minutes;
	u8 hours;
	u8 day;
	u8 month;
	u32 year;
};

void time_get(struct tm *); //Obtiene la fecha y la hora actuales.
void time_set(struct tm *); //Establece la fecha y la hora actuales.
void time_sleep(int); 		//Detiene la ejecución por algunos segundos.