#include <typedef.h>

#define CMOS_ADDRESS 	0x70
#define CMOS_DATA 		0x71

//Contents			    Register
#define REG_SECONDS 	0x00      
#define REG_MINUTES		0x02
#define REG_HOURS 		0x04
#define REG_WEEKDAY 	0x06
#define REG_DAY 		0x07
#define REG_MONTH 		0x08
#define REG_YEAR 		0x09
#define REG_CENTURY 	0x32
#define REGA_STATUS	 	0x0A //Register A
#define REGB_STATUS 	0x0B //Register B

void NMI_enable();
void NMI_disable();
void rtc_init();
u8 get_RTC_register(int);