#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>

#include "mouse_driver.h"

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned int dword;

static int running_x = -1;
static int running_y = -1;
volatile uint8_t mouse_state;

uint8_t inb(uint16_t port) {
	uint8_t _v;
	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

void outb(uint16_t port, uint8_t val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

static void mouse_wait(byte a_type) {
	dword timeout = 100000;
	if (a_type == 0) {
		while (timeout--) {
			if ((inb(0x64) & 1) == 1) {
				return;
			}
		}
		return;
	}
	else {
		while (timeout--) {
			if ((inb(0x64) & 2) == 0) {
				return;
			}
		}
		return;
	}
}

static void mouse_write(byte a) {
	//wait to be able to send a command
	mouse_wait(1);
	//tell mouse we're sending a command
	outb(0x64, 0xD4);
	//wait for final part
	mouse_wait(1);
	//write
	outb(0x60, a);
}

static byte mouse_read() {
	//get response from mouse
	mouse_wait(0);
	return inb(0x60);
}

static void _process_mouse_event(uint8_t data) {
	static sbyte mouse_byte[3];
	static byte mouse_cycle = 0;

	switch (mouse_cycle) {
		case 0:
			mouse_byte[0] = inb(0x60);
			mouse_cycle++;

			//this byte contains information about mouse state (button events)
			bool middle = mouse_byte[0] & 0x4;
			if (middle) mouse_state |= 0x4;
			else mouse_state &= ~0x4;

			bool right = mouse_byte[0] & 0x2;
			if (right) mouse_state |= 0x2;
			else mouse_state &= ~0x2;

			bool left = mouse_byte[0] & 0x1;
			if (left) mouse_state |= 0x1;
			else mouse_state &= ~0x1;

			break;
		case 1:
			mouse_byte[1] = inb(0x60);
			mouse_cycle++;

			break;
		case 2:
			mouse_byte[2] = inb(0x60);

			uint8_t state = mouse_byte[0];
			uint8_t byte = mouse_byte[1];
			int8_t rel_x = byte - ((state << 4) & 0x100);
			byte = mouse_byte[2];
			int8_t rel_y = byte - ((state << 3) & 0x100);

			//printf("Core mouse event %d %d\n", rel_x, rel_y);
			int8_t mouse_databuf[4];
			mouse_databuf[0] = state;
			mouse_databuf[1] = rel_x;
			mouse_databuf[2] = rel_y;
			//mouse_databuf[1] = mouse_byte[1];
			//mouse_databuf[2] = mouse_byte[2];
			amc_message_t* amc_msg = amc_message_construct(&mouse_databuf, sizeof(mouse_databuf));
			amc_message_send("com.axle.awm", amc_msg);
			printf("mouse driver mouse event: %d %d %d\n", state, rel_x, rel_y);

			mouse_cycle = 0;

		default:
			mouse_cycle = 0;
			break;
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.mouse_driver");

	while (true) {
		// The message from the low-level keyboard driver will contain the bare scancode
		amc_message_t msg = {0};
		amc_message_await("com.axle.core", &msg);
		uint8_t state = msg.data[0];
		uint8_t rel_x = msg.data[1];
		uint8_t rel_y = msg.data[2];
		//mouse_databuf[1] = mouse_byte[1];
		//mouse_databuf[2] = mouse_byte[2];
		int8_t mouse_databuf[4];
		mouse_databuf[0] = state;
		mouse_databuf[1] = rel_x;
		mouse_databuf[2] = rel_y;
		//mouse_databuf[1] = mouse_byte[1];
		//mouse_databuf[2] = mouse_byte[2];
		amc_message_t* amc_msg = amc_message_construct(&mouse_databuf, sizeof(mouse_databuf));
		amc_message_send("com.axle.awm", amc_msg);
		printf("mouse driver mouse event: %d %d %d\n", state, rel_x, rel_y);
		//_process_mouse_event(&msg);
		//uint8_t m0 = msg.data[0];
		//uint8_t m1 = msg.data[1];
		//uint8_t m2 = msg.data[2];
		//char chbuf[256];
		//snprintf(&chbuf, sizeof(chbuf), "Received mouse event: %d %d\n", m1, m2);
		//amc_message_t* keypress_msg = amc_message_construct(&chbuf, sizeof(chbuf));
		//amc_message_send("com.axle.awm", keypress_msg);
		//printf("Received mouse event: %d %d\n", m1, m1);
	}
	
	return 0;
}
