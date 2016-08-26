#ifndef STD_COMMON_H
#define STD_COMMON_H

#include "std_base.h"
#include <stdint.h>

__BEGIN_DECLS

typedef struct registers {
	uint32_t ds; 					//data segment selector
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;//pushed by pusha
	uint32_t int_no, err_code; 			//interrupt number and error code
	uint32_t eip, cs, eflags, useresp, ss; 		//pushed by the processor automatically
	uint32_t cr3; 					//page directory
} registers_t;

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

//force wait for i/o operation to complete
//this should only be used when there's nothing like
//a status register or IRQ to tell you info has been received
STDAPI void io_wait(void);

//returns if interrupts are on
STDAPI char interrupts_enabled(void);

//requests CPUID
STDAPI void cpuid(int code, uint32_t* a, uint32_t* d);

__END_DECLS

#endif // STD_COMMON_H
