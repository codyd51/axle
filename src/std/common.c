#include "common.h"

//write byte out to specified port
void outb(uint16_t port, uint8_t val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

void outw(uint16_t port, uint16_t val) {
	asm volatile("outb %0, %1" : : "a"(val), "dN"(port));
}

uint8_t inb(uint16_t port) {
	uint8_t _v;

	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

uint16_t inw(uint16_t port) {
	uint16_t _v;

	__asm__ __volatile__ ("inw %1, %0" : "=a" (_v) : "dN" (port));
}
