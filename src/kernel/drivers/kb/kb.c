#include "kb.h"
#include "kb_us.h"
#include <std/common.h>
#include <std/std.h>
#include <kernel/util/interrupts/isr.h>
#include <kernel/util/syscall/sysfuncs.h>
#include <kernel/util/kbman/kbman.h>

void kb_callback(registers_t regs);

keymap_t* layout;

//index into circular buffer of kb data
uint32_t kb_buffer_start;
uint32_t kb_buffer_end;
//circular buffer of kb data
char kb_buffer[256];

void kb_install() {
	printf_info("Initializing keyboard driver...");

	register_interrupt_handler(IRQ1, &kb_callback);
	switch_layout(&kb_us);

	kb_buffer_start = 0;
	kb_buffer_end = 0;
}

char kgetch() {
	//printf("getch start %d end %d\n", kb_buffer_start, kb_buffer_end);
	if (kb_buffer_start != kb_buffer_end) {
		char c = kb_buffer[kb_buffer_start++];
		//if we went out of bounds, wrap to start of buffer
		kb_buffer_start &= 255;
		return c;
	}
	//no characters available
	return '\0';
}

char getchar() {
	sys_yield(KB_WAIT);
	return kgetch();
}

bool haskey() {
	return (kb_buffer_start != kb_buffer_end);
}

void switch_layout(keymap_t* new) {
	layout = new;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void kb_callback(registers_t regs) {
	uint8_t scancode = inb(0x60);

	//check if key was released
	if (scancode & RELEASED_MASK) {
		//modifier flags stored in first 5 bits
		for (int i = 0; i < 5; i++) {
			if (layout->control_map[i] == (scancode & ~RELEASED_MASK)) {
				//releasing key always disables its function
				layout->controls &= ~(1 << i);
				return;
			}
		}

		//inform OS
		//clear released bit
		kbman_process_release(scancode ^ 0x80);
	}
	else {
		//was this a control key?
		//also invert bit in status map
		for (int i = 0; i < 8; i++) {
			if (layout->control_map[i] == scancode) {
				//if bit was set, delete it
				if (layout->controls & 1 << i) {
					layout->controls &= ~(1 << i);
				}
				//set it
				else {
					layout->controls |= 1 << i;
				}
				return;
			}
		}

		//non-control key
		//get uppercase/lowecase version depending on control keys status
		uint8_t* scancodes = layout->scancodes;
		if ((layout->controls & (LSHIFT | RSHIFT | CAPSLOCK)) && !(layout->controls & CONTROL)) {
			scancodes = layout->shift_scancodes;
		}

		//don't overflow buffer if possible :p
		if (kb_buffer_end != kb_buffer_start - 1) {
			kb_buffer[kb_buffer_end++] = scancodes[scancode];
			kb_buffer_end &= 255;
		}
		
		// if this key was a special key, inform os
		kbman_process(scancode);
	}
}
#pragma GCC diagnostic pop

