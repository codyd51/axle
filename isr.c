#include "common.h"
#include "isr.h"
#include "kernel.h"

//gets called from ASM interrupt handler stub
void isr_handler(registers_t regs) {
	printf("recieved interrupt: %d\n", regs.int_no);
}
