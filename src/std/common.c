#include "common.h"

void invlpg(void* m) {
    /* Clobber memory to avoid optimizer re-ordering access before invlpg, which may cause nasty bugs. */
    asm volatile ( "invlpg (%0)" : : "b"(m) : "memory" );
}

void outb(uint16_t port, uint8_t val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

void outw(uint16_t port, uint16_t val) {
	asm volatile("outw %0, %1" : : "a"(val), "dN"(port));
}

void outl(uint16_t port, uint32_t val) {
	asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
	uint8_t _v;
	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

uint16_t inw(uint16_t port) {
	uint16_t _v;
	__asm__ __volatile__ ("inw %1, %0" : "=a" (_v) : "dN" (port));
	return _v;
}

uint32_t inl(uint16_t port) {
	uint32_t _v;
	__asm __volatile__("inl %1, %0" : "=a" (_v) : "Nd" (port));
	return _v;
}

void insm(unsigned short port, unsigned char * data, unsigned long size) {
	asm volatile ("rep insw" : "+D" (data), "+c" (size) : "d" (port) : "memory");
}

//force wait for i/o operation to complete
//this should only be used when there's nothing like
//a status register or IRQ to tell you info has been received
void io_wait(void) {
	//TODO this is fragile
	asm volatile("		\
		jmp 1f;		\
		1: jmp 2f;	\
		2:		\
		");
}

//returns if interrupts are on
char interrupts_enabled(void) {
	unsigned long flags;
	asm volatile("	\
		pushf;	\
		pop %0;	\
		" : "=g"(flags));
	return flags & (1 << 9);
}

//requests CPUID
void cpuid(int code, uint32_t* a, uint32_t* d) {
	asm volatile("cpuid" : "=a"(*a), "=d"(*d) : "0"(code) : "ebx", "ecx");
}
