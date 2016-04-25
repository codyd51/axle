#include "clock.h"
#include "interrupt.h"

#define CURRENT_YEAR 2016

int century_register = 0x00;

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
	unsigned char century, last_second, last_minute, last_hour, last_day, last_month, last_year, last_century, registerB;

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
	if (century_register != 0) {
		century = get_RTC_register(century_register);
	}

	do {
		last_second = second;
		last_minute = minute;
		last_hour = hour;
		last_day = day;
		last_month = month;
		last_year = year;
		last_century = century;

		//make sure update isnt in progress
		while (get_update_in_progress_flag());
		second = get_RTC_register(0x00);
		minute = get_RTC_register(0x02);
		hour = get_RTC_register(0x04);
		day = get_RTC_register(0x07);
		month = get_RTC_register(0x08);
		year = get_RTC_register(0x09);
		if (century_register != 0) {
			century = get_RTC_register(century_register);
		}
	} while ((last_second != second) || (last_minute != minute) || (last_hour != hour) || (last_day != day) || (last_month != month) || (last_year != year) || (last_century != century));

	registerB = get_RTC_register(0x0B);

	//convert BCD to binary vals if necessary
	if (!(registerB & 0x04)) {
		second = (second & 0x0F) + ((second / 16) * 10);
		minute = (minute & 0x0F) + ((minute / 16) * 10);
		hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
		day = (day & 0x0F) + ((day / 16) * 10);
		month = (month & 0x0F) + ((month / 16) * 10);
		year = (year & 0x0F) + ((year / 16) * 10);
		if (century_register != 0) {
		    century = (century & 0x0F) + ((century / 16) * 10);
		}
	}

	//convert 12 hour clock to 24 hour clock if necessary
	if (!(registerB & 0x02) && (hour & 0x80)) {
		hour = ((hour & 0x7F) + 12) % 24;
	}

	//calculate full 24-digit year
	if (century_register != 0) {
		year += century * 100;
	}
	else {
		year += (CURRENT_YEAR / 100) * 100;
		if (year < CURRENT_YEAR) {
			year += 100;
		}
	}
}

void register_cmos(int reg) {
	//disable all IRQs
	__asm__ volatile("cli");

	int val = 0x70;
	outb(val, reg);

	//reenable IRQs
	__asm__ volatile("sti");
}

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

	return second;
}

void sleep(int secs) {
	int end = (int)second + secs;
	char b[10];
	itoa(end, b);
	terminal_writestring(b);
	while ((int)second < end) {
		read_rtc();
	}
}



