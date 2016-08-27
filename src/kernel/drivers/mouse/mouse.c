#include "mouse.h"
#include <kernel/util/interrupts/isr.h>
#include <std/math.h>
#include <std/std.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned int dword;

volatile int running_x = 0;
volatile int running_y = 0;
volatile uint8_t mouse_state;

Coordinate mouse_point() {
	return point_make(running_x, running_y);
}
uint8_t mouse_events() {
	return mouse_state;
}

#define VESA_WIDTH 1024
#define VESA_HEIGHT 768
void update_mouse_position(int x, int y) {
	running_x += x;
	running_x = MAX(running_x, 0);
	running_x = MIN(running_x, VESA_WIDTH);
	running_y -= y;
	running_y = MAX(running_y, 0);
	running_y = MIN(running_y, VESA_HEIGHT);
}

void mouse_callback(registers_t* regs) {
	static sbyte mouse_byte[3];
	static byte mouse_cycle = 0;

	switch (mouse_cycle) {
		case 0:
			mouse_byte[0] = inb(0x60);
			mouse_cycle++;
			break;
		case 1:
			mouse_byte[1] = inb(0x60);
			mouse_cycle++;

			//this byte contains information about mouse state (button events)
			bool middle = mouse_byte[1] & 0x4;
			if (middle) mouse_state |= 0x4;
			else mouse_state &= ~0x4;

			bool right = mouse_byte[1] & 0x2;
			if (right) mouse_state |= 0x2;
			else mouse_state &= ~0x2;

			bool left = mouse_byte[1] & 0x1;
			if (left) mouse_state |= 0x1;
			else mouse_state &= ~0x1;

			break;
		case 2:
			mouse_byte[2] = inb(0x60);
			//Bochs
			//update_mouse_position(mouse_byte[1], mouse_byte[2]);
			//QEMU
			update_mouse_position(mouse_byte[2], mouse_byte[0]);
			mouse_cycle = 0;
			break;
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

void mouse_install() {
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

	//enable data reporting 
	mouse_write(0xF4);
	mouse_read(); //acknowledge

	//setup mouse handler
	register_interrupt_handler(IRQ12, &mouse_callback);
}
