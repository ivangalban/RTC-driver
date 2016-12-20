#ifndef __RTC_H__
#define __RTC_H__

#include <typedef.h>
#include <vfs.h>
#include <string.h>

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

//Formats of the date/time RTC bytes:
#define BINARY_MODE		0x04
#define FORMAT_24HOURS	0x02

//ID
#define RTC_MAJOR		13
#define RTC_MINOR 		17


vfs_file_t *fdrtc;
//u8 REGISTER[] = {REG_SECONDS, REG_MINUTES, REG_HOURS,  REG_DAY, REG_MONTH, REG_YEAR};
//#define REGISTER_COUNT strlen(REGISTER)

void NMI_enable();
void NMI_disable();
void rtc_init();
u8 get_RTC_register(u8);
void set_RTC_register(u8, u8);
int get_update_in_progress_flag();

#endif