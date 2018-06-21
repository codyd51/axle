#ifndef STD_COMMON_H
#define STD_COMMON_H

#include "std_base.h"
#include <stdint.h>
#include <kernel/interrupts/idt_structures.h>

__BEGIN_DECLS

#define PAGE_SIZE 0x1000
#define BITS_PER_BYTE 8

#define kernel_begin_critical() __asm__("cli");
#define kernel_end_critical() __asm__("sti");

typedef register_state_t registers_t;


//write byte to port
STDAPI void outb(uint16_t port, uint8_t val);
//write word to port
STDAPI void outw(uint16_t port, uint16_t val);
//write 32bits to port
STDAPI void outl(uint16_t port, uint32_t val);

//read byte from port
STDAPI uint8_t inb(uint16_t port);
//read word from port
STDAPI uint16_t inw(uint16_t port);
//read 32bits from port
STDAPI uint32_t inl(uint16_t port);

STDAPI void insm(unsigned short port, unsigned char * data, unsigned long size);

#define insl(port, buffer, count) \
         __asm__ ("cld; rep; insl" :: "D" (buffer), "d" (port), "c" (count))

#define insw(port, buffer, count) \
		__asm__ ("rep insw" :: "D"(buffer), "d"(port), "c"(count))

#define insb(port, buffer, count) \
		__asm__ ("rep insb" :: "D"(buffer), "d"(port), "c"(count))

#define outsw(port, buffer, count) \
		__asm__ ("rep outsw" :: "c"(count), "d"(port), "S"(edi))

//force wait for i/o operation to complete
//this should only be used when there's nothing like
//a status register or IRQ to tell you info has been received
STDAPI void io_wait(void);

//returns if interrupts are on
STDAPI char interrupts_enabled(void);

//requests CPUID
STDAPI void cpuid(int code, uint32_t* a, uint32_t* d);

//invalidate TLB entry associated with virtual address @p m
void invlpg(void* m);

__END_DECLS

#endif // STD_COMMON_H
