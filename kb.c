#include "kb.h"
#include "kernel.h"
#include "interrupt.h"

#define INT_DISABLE 0
#define INT_ENABLE  0x200
#define PIC1 0x20
#define PIC2 0xA0

#define ICW1 0x11
#define ICW4 0x01

//TODO implement bitmask for special keys (shift/ctrl/fn/etc)

/* KBDUS means US Keyboard Layout. This is a scancode table
*  used to layout a standard US keyboard. I have left some
*  comments in to give you an idea of what key is what, even
*  though I set it's array index to 0. You can change that to
*  whatever you want using a macro, if you wish! */
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

void init_pics(int pic1, int pic2) {
	 //send ICW1
	 outb(PIC1, ICW1);
	 outb(PIC2, ICW1);

	 //send ICW2
	 outb(PIC1 + 1, pic1);   
	 outb(PIC2 + 1, pic2);   

	 //send ICW3
	 outb(PIC1 + 1, 4);   
	 outb(PIC2 + 1, 2);

	 //send ICW4
	 outb(PIC1 + 1, ICW4);
	 outb(PIC2 + 1, ICW4);

	 //disable all IRQs
	 outb(PIC1 + 1, 0xFF);
}

void init_kb() {
	//set up keyboard handshake
	init_pics(0x20, 0x28);
}

bool hasKeypressFinished = false;
bool hasShift = false;

//handles keyboard interrupts
char getchar() {
	unsigned char c = 0;
	hasKeypressFinished = false;
	while (1) {
		//read from keyboard's data buffer
		if (inb(0x60) != c) {
			//0x60 is port from which we read
			c = inb(0x60);

			char mappedchar = kbdus[c];

			//if the top bit of the byte we read from the KB is set, then a key's just been released
			if (c & 0x80) {
				//TODO scan to see if user released shift/alt/control keys
				hasKeypressFinished = true;

				//if shift was just released, reset hasShift
				c = c ^ 0x80;
				if (c == 42 || c == 54) {
					hasShift = false;
					continue;
				}
			}
			else if (c > 0 && hasKeypressFinished) {
				//we got a keypress
				//repeated keypresses will generate multiple interrupts

				//detect shift
				if (c == 42 || c == 54) {
					hasShift = true;
					continue;
				}

				//reset for next use
				hasKeypressFinished = false;

				if (hasShift) return toupper(mappedchar);
				return mappedchar;
			}
		}
	}
	return NULL;
}






