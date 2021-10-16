#include "serial.h"

//COM 1
#define PORT 0x3F8

#define BUF_SIZE 1028*8
static char buffer[BUF_SIZE];
static int idx = 0;

void outb(uint16_t port, uint8_t val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

uint8_t inb(uint16_t port) {
	uint8_t _v;
	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

void *memset(void *s, int c, uint64_t n)
{
    uint8_t *p=(uint8_t*)s;
    if(s && n>0) {
        while(n--) *p++ = c;
    }
    return s;
}


int serial_waiting() {
	return inb(PORT + 5) & 1;
}

char serial_get() {
	while (serial_waiting() == 0);
	return inb(PORT);
}

bool is_transmitting() {
	return inb(PORT + 5) & 0x20;
}

void __serial_putchar(char c) {
	while (is_transmitting() == 0);
	outb(PORT, c);
}

void __serial_writestring(char* str) {
	char* ptr = str;
	while (*ptr) {
		__serial_putchar(*(ptr++));
	}
}

static void serial_flush() {
	__serial_writestring(buffer);
	memset(buffer, 0, BUF_SIZE);
	idx = 0;
}

void serial_putchar(char c) {
	if (idx + 1 >= BUF_SIZE) {
		//buffer full, flush to real serial
		serial_flush();
	}
	//append c to buffer
	buffer[idx+0] = c;
	buffer[idx+1] = '\0';
	idx++;
	//also flush on newline
	if (c == '\n') {
		serial_flush();
	}
}

void serial_puts(char* str) {
	//is this string too big to be directly conc'd with buffer?
	//idx == strlen(buffer)
    /*
	if (idx + strlen(str) < BUF_SIZE) {
		strcat(buffer, str);
		idx += strlen(str);
	}
	else {
    */
		char* ptr = str;
		while (*ptr) {
			serial_putchar(*(ptr++));
		}
        //serial_flush();
	//}
}

void serial_init() {
	memset(buffer, 0, BUF_SIZE);

	outb(PORT + 1, 0x00); //interrupts off
	outb(PORT + 3, 0x80); //baud rate
	outb(PORT + 0, 0x03); //divisor to 3
	outb(PORT + 1, 0x00);
	outb(PORT + 3, 0x03); //1 byte, no parity, 1 stop bit
	outb(PORT + 2, 0xC7); //FIFO, 14-byte threshold
	outb(PORT + 4, 0x0B); //irq on, RTS/DSR set
}
