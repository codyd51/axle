#include "kb.h"
#include "kernel.h"

#define INT_DISABLE 0
#define INT_ENABLE  0x200
#define PIC1 0x20
#define PIC2 0xA0

#define ICW1 0x11
#define ICW4 0x01

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

void outb(unsigned short port, unsigned char val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

static __inline unsigned char inb (unsigned short int port) {
	unsigned char _v;

	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

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

//handles keyboard interrupt
/*
void keyboard_handler(struct regs* r) {
	unsigned char scancode;

	//read from keyboard's data buffer
	scancode = inportb(0x60);

	//if the top bit of the byte we read from the KB is set, then a key has just been released
	if (scancode & 0x80) {
		//scan to see if user released shift/alt/control keys TODO
	}
	else {
		//a key was just pressed!
		//repeated keypress will generate multiple interrupts

		//just translate KB scancode into an ASCII value, then display to the screen.
		terminal_putchar(kbdus[scancode]);
	}
}
*/

char* get_input() {
	char* ret = malloc(sizeof(char) * 256);

	char c = 0;
	init_pics(0x20, 0x28);
	do {
		//read from keyboard's data buffer
		if (inb(0x60) != c) {
			//0x60 is port from which we read
				c = inb(0x60);

				//if the top bit of the byte we read from the KB is set, then a key's just been released
				//if (c & 0x80) {
					//TODO scan to see if user released shift/alt/control keys
				//}
				//else if (c > 0) {
				if (c > 0) {
					//we got a keypress

					//add this character to the input string
					strccat(ret, kbdus[c]);
					//print this mapped character to the screen
					terminal_putchar(kbdus[c]);
					
					//if we don't print out ret everything breaks
					//TODO figure out why
					//terminal_writestring("\n");
					terminal_writestring(ret);
					//terminal_writestring("\n");
				}
			}
	}
	while (c != 28); // 28 = enter

	return ret;
}






