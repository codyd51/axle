#include "clock.h"
#include "interrupt.h"

unsigned char second, minute, hour, day, month, year;

enum {
	cmos_address = 0x70,
	cmos_data =    0x71
};

int get_update_in_progress_flag() {
	outb(cmos_address, 0x0A);
	return (inb(cmos_data) & 0x80);
}

unsigned char get_RTC_register(int reg) {
	outb(cmos_address, reg);
	return inb(cmos_data);
}

void read_rtc() {
	unsigned char last_second, last_minute, last_hour, last_day, last_month, last_year, last_century, registerB;

	//note: we use the 'read registers until we get the same value twice' method to
	//avoid getting inconsistent values due to RTC updates

	//make sure an update isn't in progress
	while (get_update_in_progress_flag());
	second = get_RTC_register(0x00);
	minute = get_RTC_register(0x02);
	hour = get_RTC_register(0x04);
	day = get_RTC_register(0x07);
	month = get_RTC_register(0x08);
	year = get_RTC_register(0x09);

	do {
		last_second = second;
		last_minute = minute;
		last_hour = hour;
		last_day = day;
		last_month = month;
		last_year = year;

		//make sure update isnt in progress
		while (get_update_in_progress_flag());
		second = get_RTC_register(0x00);
		minute = get_RTC_register(0x02);
		hour = get_RTC_register(0x04);
		day = get_RTC_register(0x07);
		month = get_RTC_register(0x08);
		year = get_RTC_register(0x09);
	} while ((last_second != second) || (last_minute != minute) || (last_hour != hour) || (last_day != day) || (last_month != month) || (last_year != year));

	registerB = get_RTC_register(0x0B);

	//convert BCD to binary vals if necessary
	if (!(registerB & 0x04)) {
		second = (second & 0x0F) + ((second / 16) * 10);
		minute = (minute & 0x0F) + ((minute / 16) * 10);
		hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
		day = (day & 0x0F) + ((day / 16) * 10);
		month = (month & 0x0F) + ((month / 16) * 10);
		year = (year & 0x0F) + ((year / 16) * 10);
	}

	//convert 12 hour clock to 24 hour clock if necessary
	if (!(registerB & 0x02) && (hour & 0x80)) {
		//hour = ((hour & 0x7F) + 12) % 24;
	}

	//calculate full 24-digit year
	/*
	if (century_register != 0) {
		year += century * 100;
	}
	else {
		year += (CURRENT_YEAR / 100) * 100;
		if (year < CURRENT_YEAR) {
			year += 100;
		}
	}
	*/
}

void register_cmos(int reg) {
	//disable all IRQs
	__asm__ volatile("cli");

	int val = 0x70;
	outb(val, reg);

	//reenable IRQs
	__asm__ volatile("sti");
}

unsigned char epoch_time();
unsigned char time() {
	read_rtc();

	terminal_writestring("\n");
	
	char b[100];
	itoa(hour, b);
	terminal_writestring(b);
	terminal_writestring(":");

	itoa(minute, b);
	terminal_writestring(b);
	terminal_writestring(":");

	itoa(second, b);
	terminal_writestring(b);

	terminal_writestring(", ");

	itoa(month, b);
	terminal_writestring(b);
	terminal_writestring("/");

	itoa(day, b);
	terminal_writestring(b);
	terminal_writestring("/");

	itoa(year, b);
	terminal_writestring(b);

	return epoch_time();
}

bool is_leap_year(int year) {
	return (year % 4 == 0);
}

int days_in_year(int year) {
	if (is_leap_year(year)) return 366;
	return 365;
}

int days_in_month(int month, int year) {
	switch (month) {
		case 0:
			return 31;
			break;
		case 1:
			if (is_leap_year(year)) return 29;
			return 28;
			break;
		case 2:
			return 31;
			break;
		case 3:
			return 30;
			break;
		case 4:
			return 31;
			break;
		case 5:
			return 30;
			break;
		case 6:
			return 31;
			break;
		case 7:
			return 31;
			break;
		case 8:
			return 30;
			break;
		case 9:
			return 31;
			break;
		case 10:
			return 30;
			break;
		case 11:
		default:
			return 31;
			break;
	}
	return -1;
}

unsigned char epoch_time() {
	//get current time
	read_rtc();

	size_t secs_since_1970 = 0;

	//compute years since 1970
	int passed_years = 30 + year;
	//year = 60s * 60m * 24h * 365d
	for (int i = 1; i <= passed_years; i++) {
		int currYear = year - i;
		secs_since_1970 += (days_in_year(currYear) * 24 * 60 * 60);
	}

	//compute months in year
	//month = 60s * 60m * 24h
	for (int i = 0; i < month; i++) {
		secs_since_1970 += (days_in_month(i, year) * 24 * 60 * 60);
	}

	//compute days in month
	//day = 60s * 60m * 24h
	secs_since_1970 += day * 24 * 60 * 60;

	//compute hours in day
	//hour = 60s * 60m
	secs_since_1970 += hour * 60 * 60;

	//compute minutes in hour
	//minutes = 60s
	secs_since_1970 += minute * 60;

	//compute seconds in minute
	//seconds = seconds
	secs_since_1970 += second;

	return secs_since_1970;
}

void sleep(int secs) {
	int end = epoch_time() + secs;
	while (epoch_time() < end) {}
}



