#include "kb.h"
#include "kb_us.h"
#include "kb_colemak.h"
#include <std/common.h>
#include <std/std.h>
#include <kernel/interrupts/interrupts.h>
#include <kernel/syscall/sysfuncs.h>
#include <kernel/util/kbman/kbman.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/util/unistd/unistd.h>

void kb_callback(registers_t* regs);

keymap_t* layout;

void kb_install() {
	printf_info("Initializing keyboard driver...");

	interrupt_setup_callback(INT_VECTOR_IRQ1, &kb_callback);
	switch_layout(&kb_colemak);
}

char kgetch() {
	char ch;
	int c = read(0, &ch, 1);
	if (!c || ch == -1) {
		//either read error or
		//no characters available
		return '\0';
	}
	return ch;
}

char getchar() {
	sys_yield(KB_WAIT);
	return kgetch();
}

bool haskey() {
	if (!tasking_is_active()) {
		return false;
	}
	task_t* current = task_with_pid(getpid());
	if (!current) {
		return false;
	}
	return !!current->std_stream->buf->count;
}

void switch_layout(keymap_t* new) {
	layout = new;
}

key_status_t kb_modifiers() {
	return layout->controls;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void kb_callback(registers_t* regs) {
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
		scancode &= ~RELEASED_MASK;
		//kbman_process_release(layout->scancodes[scancode]);
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

		task_small_t* first_responder = get_first_responder();
		if (first_responder) {
			std_stream_pushchar(first_responder->stdin_stream, scancodes[scancode]);
			// TODO(PT): Instead of running iosentinel on all tasks,
			// we could send some sort of signal that this specific task can be unblocked
			// due to a keyboard event.
			iosentinel_check_now();
		}
		else {
			printf("Unrouted keystroke: %c\n", scancodes[scancode]);
		}

		//inform OS of keypress
		//kbman_process(scancodes[scancode]);
	}
}
#pragma GCC diagnostic pop
