#include "kb.h"
#include <kernel/kernel.h>
#include <std/common.h>
#include <std/std.h>
#include <kernel/util/interrupts/isr.h>
#include <kernel/util/kbman/kbman.h>
#include <kernel/util/mutex/mutex.h>

#define KBD_DATA_PORT 0x60

//TODO implement bitmask for special keys (shift/ctrl/fn/etc)
static const unsigned short shiftMask = 4;
static const unsigned short keypressFinishedMask = 2;
static unsigned int flags = 0;

static array_m* kb_buffer;
static lock_t* mutex;

//KBDUS means US Keyboard Layout. This is a scancode table
//used to layout a standard US keyboard.
unsigned char kbdus[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
		0,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
		0,	/* Alt */
	' ',	/* Space bar */
		0,	/* Caps lock */
		0,	/* 59 - F1 key ... > */
		0,   0,   0,   0,   0,   0,   0,   0,
		0,	/* < ... F10 */
		0,	/* 69 - Num lock*/
		0,	/* Scroll Lock */
		0,	/* Home key */
		0,	/* Up Arrow */
		0,	/* Page Up */
	'-',
		0,	/* Left Arrow */
		0,
		0,	/* Right Arrow */
	'+',
		0,	/* 79 - End key*/
		0,	/* Down Arrow */
		0,	/* Page Down */
		0,	/* Insert Key */
		0,	/* Delete Key */
		0,   0,   0,
		0,	/* F11 Key */
		0,	/* F12 Key */
		0,	/* All other keys are undefined */
};

void add_character_to_buffer(char ch) {
	lock(mutex);
	array_m_insert(kb_buffer, ch);
	unlock(mutex);
}

void kb_callback(registers_t regs) {
	static unsigned char c = 0;
	
	//read from keyboard's data buffer
	if (inb(KBD_DATA_PORT) != c) {
		c = inb(KBD_DATA_PORT);

		//if top byte we read from KB is set, 
		//then a key was just released
		if (c & 0x80) {
			// If the key released was shift, then remove the mask
			if ((c == 170 || c == 182) && (flags & shiftMask)) {
				flags = flags ^ shiftMask;
			}
			return;
		}
		char mappedchar = kbdus[c];

		//TODO scan to see if suer released shift/alt/control keys
		flags = flags | keypressFinishedMask;

		// If shift is detected and the mask hasn't been added, add it
		if ((c == 42 || c == 54) && !(flags & shiftMask)) {
			flags = flags ^ shiftMask;
			return;
		}

		// if this key was a special key, inform os
		// TODO dedicated function to check for special keys 
		kbman_process(c);

		//rest for next use
		flags = flags ^ keypressFinishedMask;

		if (flags & shiftMask) {
			if (toupper(mappedchar) == mappedchar) {
				mappedchar = toupper_special(mappedchar);
			} else {
				mappedchar = toupper(mappedchar);
			}
		}
		add_character_to_buffer(mappedchar);
	}
}

char toupper_special(char character) {
	switch(character) {
		case '`':
			return '~';
		case '1':
			return '!';
		case '2':
			return '@';
		case '3':
			return '#';
		case '4':
			return '$';
		case '5':
			return '%';
		case '6':
			return '^';
		case '7':
			return '&';
		case '8':
			return '*';
		case '9':
			return '(';
		case '0':
			return ')';
		case '-':
			return '_';
		case '=':
			return '+';
		case '[':
			return '{';
		case ']':
			return '}';
		case '\\':
			return '|';
		case ';':
			return ':';
		case '\'':
			return '"';
		case ',':
			return '<';
		case '.':
			return '>';
		case '/':
			return '?';
		default:
			return character;
	}
}

void kb_install() {
	printf_info("Initializing keyboard driver...");

	kb_buffer = array_m_create(256);
	mutex = lock_create();

	register_interrupt_handler(IRQ1, &kb_callback);
}

int haskey() {
	return kb_buffer->size > 0;
}

//does not block!
char kgetch() {
	lock(mutex);

	//return last character from KB buffer, and remove that character
	char ret = array_m_lookup(kb_buffer, kb_buffer->size - 1);
	array_m_remove(kb_buffer, kb_buffer->size - 1);
	
	unlock(mutex);
	return ret;
}

//blocks until character is received
char getchar() {
	while (!haskey()) {}
	return kgetch();
}






