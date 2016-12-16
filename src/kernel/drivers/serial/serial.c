#include "serial.h"

//COM 1
#define PORT 0x3F8 

void serial_init() {
	printf_info("Initializing serial driver...");

	outb(PORT + 1, 0x00); //interrupts off
	outb(PORT + 3, 0x80); //baud rate
	outb(PORT + 0, 0x03); //divisor to 3 
	outb(PORT + 1, 0x00); 
	outb(PORT + 3, 0x03); //1 byte, no parity, 1 stop bit
	outb(PORT + 2, 0xC7); //FIFO, 14-byte threshold
	outb(PORT + 4, 0x0B); //irq on, RTS/DSR set
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

void serial_putchar(char c) {
	while (is_transmitting() == 0);
	outb(PORT, c);
}

void serial_writestring(char* str) {
	char* ptr = str;
	while (*ptr) {
		serial_putchar(*(ptr++));
	}
}

