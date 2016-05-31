#include "mouse.h"
#include <kernel/util/interrupts/isr.h>
#include <std/math.h>

typedef unsigned char byte;
typedef signed char sbyts;
typedef unsigned int dword;

int running_x;
int running_y;

int mouse_x() {
	return running_x;
}
int mouse_y() {
	return running_y;
}

void update_mouse_position(int x, int y) {
	running_x += x;
	running_x = MIN(running_x, 500);
	running_y += y;
	running_y = MIN(running_y, 500);
	printf_info("{%d,%d}", running_x, running_y);
}

void mouse_callback(registers_t* regs) {
	static unsigned char cycle = 0;
	static char mouse_bytes[3];
	mouse_bytes[cycle++] = inb(0x60);
	if (cycle == 3) {
		//we have all 3 bytes
		//reset counter
		cycle = 0;

		if ((mouse_bytes[0] & 0x80) || (mouse_bytes[0] & 0x40)) {
			//mouse only sends overflow info
			//we don't care about this
			return;
		}

		update_mouse_position(mouse_bytes[1], mouse_bytes[2]);
	}
}

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

void mouse_write(byte a) {
	//wait to be able to send a command
	mouse_wait(1);
	//tell mouse we're sending a command
	outb(0x64, 0xD4);
	//wait for final part
	mouse_wait(1);
	//write
	outb(0x60, a);
}

byte mouse_read() {
	//get response from mouse
	mouse_wait(0);
	return inb(0x60);
}

void initialize_mouse() {
	byte status;

	//enable mouse device
	mouse_wait(1);
	outb(0x64, 0xA8);

	//enable interrupts
	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	status = (inb(0x60) | 2);
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, status);

	//tell mouse to use default settings
	mouse_write(0xF6);
	mouse_read(); //acknowledge

	//enable mouse
	mouse_write(0xF4);
	mouse_read(); //acknowledge

	//setup mouse handler
	register_interrupt_handler(IRQ12, &mouse_callback);
}
