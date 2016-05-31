#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

//write byte to port
void outb(uint16_t port, uint8_t val);
//write word to port
void outw(uint16_t port, uint16_t val);
//write 32bits to port
void outl(uint16_t port, uint32_t val);

//read byte from port
uint8_t inb(uint16_t port);
//read word from port
uint16_t inw(uint16_t port);
//read 32bits from port
uint32_t inl(uint16_t port);

//force wait for i/o operation to complete
//this should only be used when there's nothing like
//a status register or IRQ to tell you info has been received
void io_wait();

//returns if interrupts are on
char interrupts_enabled();

//requests CPUID
void cpuid(int code, uint32_t* a, uint32_t* d);

#endif
