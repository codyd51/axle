#include "clock.h"
#include <std/common.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/util/multitasking/tasks/task.h>

time_t current_time;

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

unsigned char read_rtc_register(unsigned char reg) {
	outb(0x70, reg);
	return inb(0x71);
}

unsigned char bcd2bin(unsigned char bcd) {
	return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static bool bcd;
static void handle_rtc_update() {
	bool ready = read_rtc_register(0x0C) & 0x10;
    if(ready){
        if(bcd){
            current_time.second = bcd2bin(read_rtc_register(0x00));
            current_time.minute = bcd2bin(read_rtc_register(0x02));
            current_time.hour   = bcd2bin(read_rtc_register(0x04));
            current_time.month  = bcd2bin(read_rtc_register(0x08));
            current_time.year   = bcd2bin(read_rtc_register(0x09));
            current_time.day_of_month = bcd2bin(read_rtc_register(0x07));
        }
		else {
            current_time.second = read_rtc_register(0x00);
            current_time.minute = read_rtc_register(0x02);
            current_time.hour   = read_rtc_register(0x04);
            current_time.month  = read_rtc_register(0x08);
            current_time.year   = read_rtc_register(0x09);
            current_time.day_of_month = read_rtc_register(0x07);
        }
    }
	printk("heartbeat\n");
	if (tasking_installed()) {
		proc();
	}
}

void rtc_install() {
	unsigned char status = read_rtc_register(0x0B);
	status |=  0x02;				//24h clock
	status |=  0x10;				//update ended interrupts
	status &= ~0x20;				//no alarm interrupts
	status &= ~0x40;				//no periodic interrupt
	bcd		= !(status & 0x04);		//check if it's BCD format
	
	//write status to RTC
	outb(0x70, 0x0B);
	outb(0x71, status);

	//read status from RTC
	read_rtc_register(0x0C);

	register_interrupt_handler(40, handle_rtc_update);
}

#include <kernel/drivers/pit/pit.h>
uint32_t time() {
	return tick_count();
}

uint32_t time_unique() {
	/*
	static uint32_t seen[UINT8_MAX] = {0};
	//every time we skip an id we lose a millisecond
	//keep track of how many ids we threw away so we can keep track of what time we should report
	static uint32_t slide = 0;

	//increment counter of this stamp
	seen[time()]++;
	//have we seen this stamp more than just this time?
	if (seen[time()] > 1) {
		uint32_t seen_count = seen[time()];
		slide++;

		//first id was already unique, so don't add that one
		return time() + (seen_count - 1) + slide;
	}
	return time() + slide;
	*/
	return time();
}

void date(char* res) {
	char b[8];
	itoa(current_time.hour, b);
	strcat(res, b);
	strcat(res, ":");

	itoa(current_time.minute, b);
	strcat(res, b);
	strcat(res, ":");

	itoa(current_time.second, b);
	strcat(res, b);
	strcat(res, ", ");

	itoa(current_time.month, b);
	strcat(res, b);
	strcat(res, "/");

	itoa(current_time.day_of_month, b);
	strcat(res, b);
	strcat(res, "/");

	//RTC has buggy year value, so if year seems bad print -1
	//TODO update this in 8000 years
	int usable_year = current_time.year;
	if (usable_year >= 10000) usable_year = -1;
	itoa(usable_year, b);
	strcat(res, b);
}

uint32_t epoch_time() {
	//TODO write
	return 0;
}

