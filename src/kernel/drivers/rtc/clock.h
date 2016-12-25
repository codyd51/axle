#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H

#include <std/std.h>

typedef struct time_t {
	unsigned char second;
	unsigned char minute;
	unsigned char hour;
	unsigned char day_of_week;
	unsigned char day_of_month;
	unsigned char month;
	unsigned char year;
} time_t;

//install rtc driver
void rtc_install();

//return system time with millisecond precision
uint32_t time();
//to be used when unique id's are required, but accuracy is not.
//if time_unique() is called on the same timestamp more than once, it gaurantees the later request(s)
//have a unique id of their own.
uint32_t time_unique();
//UNIX timestamp
uint32_t epoch_time();
//get individual components
void gettime(time_t* time);

//formatted timestamp
//user must provide sufficiently long buffer to store date
void date(char* dest);

#endif
