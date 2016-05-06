#include "common.h"

//write byte out to specified port
void outb(u16int port, u8int val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

u8int inb(u16int port) {
	u8int _v;

	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

u16int inw(u16int port) {
	u16int _v;

	__asm__ __volatile__ ("inw %1, %0" : "=a" (ret) : "dN" (port));
}
