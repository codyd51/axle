#include "mouse.h"
#include <kernel/interrupts/interrupts.h>
#include <std/math.h>
#include <std/std.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/syscall/sysfuncs.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned int dword;

static int running_x = -1;
static int running_y = -1;
volatile uint8_t mouse_state;

static inline uint32_t log2(const uint32_t x) {
	uint32_t y;
	asm ( "\tbsr %1, %0\n"
			: "=r"(y)
			: "r" (x)
		);
	return y;
}

static inline Size screen_dimensions() {
#define VESA_WIDTH 1024
#define VESA_HEIGHT 768
	Screen* s = gfx_screen();
	int screen_width, screen_height;
	if (s) {
		screen_width = s->resolution.width;
		screen_height = s->resolution.height;
	}
	else {
		//fall back on assuming 1024x768
		screen_width = VESA_WIDTH;
		screen_height = VESA_HEIGHT;
	}
	return size_make(screen_width, screen_height);
}

Point mouse_point() {
	static int prev_running_x = 0;
	static int prev_running_y = 0;

	int delt_x = prev_running_x - running_x;
	int delt_y = prev_running_y - running_y;

	//scale factor = log base 2 of delt-x + delt-y
	//provides logarithmic acceleration
	int scaling = log2(delt_x + delt_y);
	delt_x *= scaling;
	delt_y *= scaling;

	Point new_pos = point_make(running_x + delt_x, running_y + delt_y);
	Size dimensions = screen_dimensions();

	new_pos.x = MAX(new_pos.x, 0);
	new_pos.x = MIN(new_pos.x, dimensions.width - 5);
	new_pos.y = MAX(new_pos.y, 0);
	new_pos.y = MIN(new_pos.y, dimensions.height - 5);

	prev_running_x = running_x;
	prev_running_y = running_y;

	return point_make(running_x, running_y);
}
uint8_t mouse_events() {
	return mouse_state;
}

void update_mouse_position(int x, int y) {
	//set initial mouse position if necessary
	if (running_x == -1 && running_y == -1) {
		Screen* s = gfx_screen();
		if (s) {
			running_x = s->resolution.width / 2;
			running_y = s->resolution.height / 2;
		}
		else {
			//fall back on putting cursor at origin
			running_x = running_y = 0;
		}
	}

	y = -y;

	running_x += x;
	running_y += y;

	Size dimensions = screen_dimensions();
	running_x = MAX(running_x, 0);
	running_x = MIN(running_x, dimensions.width - 5);
	running_y = MAX(running_y, 0);
	running_y = MIN(running_y, dimensions.height - 5);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void mouse_callback(registers_t regs) {
	kernel_begin_critical();

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
			update_mouse_position(mouse_byte[1], mouse_byte[2]);
			mouse_cycle = 0;

			//hook into task switch
			//trigger iosentinel
			kernel_end_critical();
			extern void update_blocked_tasks();
			update_blocked_tasks();
		default:
			mouse_cycle = 0;
			break;
	}
	kernel_end_critical();
}
#pragma GCC diagnostic pop

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
	interrupt_setup_callback(INT_VECTOR_IRQ12, &mouse_callback);
}

void mouse_event_wait() {
	sys_yield(MOUSE_WAIT);
}
