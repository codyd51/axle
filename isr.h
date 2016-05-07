#include "common.h"

typedef struct registers {
	u32int ds; 					//data segment selector
	u32int edi, esi, ebp, esp, ebx, edx, ecx, eax;  //pushed by pusha
	u32int int_no, err_code; 			//interrupt number and error code
	u32int eip, cs, eflags, useresp, ss; 		//pushed by the processor automatically
} registers_t;
