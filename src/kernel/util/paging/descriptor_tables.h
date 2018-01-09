#include <std/common.h>

//allows kernel stack in TSS to be changed
void set_kernel_stack(uint32_t stack);

//publicly accessible initialization function
void descriptor_tables_install();
void idt_install();

//struct describing task state segment
struct tss_entry_struct {
	uint32_t prev_tss; 	//previous TSS; would be used if we used hardware task switching
	uint32_t esp0;		//stack pointer to load when changing to kernel mode
	uint32_t ss0;		//stack segment to load when changing to kernel mode
	uint32_t esp1;		//unused...
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;		//value to load into ES when changing to kernel mode
	uint32_t cs; 		//as above...
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;		//unused...
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed));
typedef struct tss_entry_struct tss_entry_t;
 
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
