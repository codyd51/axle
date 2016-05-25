#include "mouse.h"
#include <kernel/util/interrupts/isr.h>
#include <kernel/kernel.h>

#include <std/common.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned int dword;

int x;
int y;
void pic_acknowledge(unsigned int interrupt);
void mouse_callback(registers_t regs) {
	/*
	static unsigned char cycle = 0;
	static char mouse_bytes[3];

	mouse_bytes[cycle++] = inb(0x60);

	if (cycle == 3) {
		//we have all 3 bytes
		//reset counter
		cycle = 0;

		if ((mouse_bytes[0] & 0x80) || (mouse_bytes[0] & 0x40)) {
			//mouse only sends overflowing info
			//we don't care about this
			return;
		}

		//button press!
		//TODO fill this in
		
		x = mouse_bytes[1]; //amount mouse moves in x axis
		y = mouse_bytes[2]; //amount mouse moves in y axis

		static int running_x = 0;
		static int running_y = 0;
		int x_movement = abs(x) + abs(running_x);
		int y_movement = abs(y) + abs(running_y);
	}
	*/
	printf_info("Got mouse byte");
	pic_acknowledge(IRQ12);
}

int mouse_X() {
	return x;
}
int mouse_Y() {
	return y;
}
/*
void mouse_wait(byte a_type) {
	unsigned int _time_out = 100000;
	if (a_type = 0) {
		while (_time_out--) {
			//data
			if ((inb(0x64) & 1) == 1) {
				printf_info("Mouse timeout data");
				return;
			}
		}
		return;
	}
	else {
		while (_time_out--) {
			//signal
			if ((inb(0x64) & 2) == 0) {
				printf_info("Mouse timeout signal");
				return;
			}
		}
		return;
	}
}

void mouse_write(byte a_write) {
	mouse_wait(1);

	outb(0x64, 0xD4);

	mouse_wait(1);

	outb(0x60, a_write);
}

unsigned char mouse_read() {
	//get response from mouse
	mouse_wait(0);
	return inb(0x60);
}
*/

void mouse_wait(byte a_type) {
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

void mouse_write(byte a_write) {
	mouse_wait(1);
	outb(0x64, 0xD4);
	mouse_wait(1);
	outb(0x60, a_write);
}

byte mouse_read() {
	mouse_wait(0);
	return inb(0x60);
}

void initialize_mouse() {
	byte status;

	mouse_wait(1);
	outb(0x64, 0xA8);

	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	status = (inb(0x60) | 2);
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, status);

	mouse_write(0xF6);
	mouse_read();

	mouse_write(0xF4);
	mouse_read();

	register_interrupt_handler(IRQ12, &mouse_callback);
}
