#include "kb.h"
#include "shell.h"
#include <stddef.h>
#include <stdint.h>

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
	 /* send ICW1 */
	 outb(PIC1, ICW1);
	 outb(PIC2, ICW1);

	 /* send ICW2 */
	 outb(PIC1 + 1, pic1);   
	 outb(PIC2 + 1, pic2);   

	 /* send ICW3 */
	 outb(PIC1 + 1, 4);   
	 outb(PIC2 + 1, 2);

	 /* send ICW4 */
	 outb(PIC1 + 1, ICW4);
	 outb(PIC2 + 1, ICW4);

	 /* disable all IRQs */
	 outb(PIC1 + 1, 0xFF);
}

char* strcat(char *dest, const char *src) {
	size_t i,j;
	for (i = 0; dest[i] != '\0'; i++)
			;
	for (j = 0; src[j] != '\0'; j++)
			dest[i+j] = src[j];
	dest[i+j] = '\0';
	return dest;
}

extern char* itoa(int i, char b[]);

char* strccat(char* dest, char src) {
	size_t i;
	for (i = 0; dest[i] != '\0'; i++)
		;
	dest[i] = src;
	dest[i+1] = '\0';
	return dest;
}

static char memory_data[32768];
static char *mem_end;

void initmem(void) {
  mem_end = memory_data;
}

void *malloc(int size) {
  char *temp = mem_end;
  mem_end += size;
  return (void*) temp;
}

void free(void *ptr) {
  /* Don't bother to free anything--if programs need to start over, they
     can re-invoke initmem */
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
	char c = 0;
	initmem();
	char* ret = malloc(sizeof(char) * 256);
	//ret = strccat(ret, 'c');

	init_pics(0x20, 0x28);
	do {
		//PORT FROM WHICH WE READ
		if (inb(0x60) != c) {
				c = inb(0x60);
				if (c > 0) {
					terminal_putchar(kbdus[c]); //print on screen
					strccat(ret, kbdus[c]);
					terminal_writestring("\nret is now: ");
					terminal_writestring(ret);
				}
			}

	}
	while(c!=28); // 1= ESCAPE

	char* asPointer = &ret[0];
	terminal_writestring("\nasPointer: ");
	terminal_writestring(asPointer);
	return asPointer;
}






