#include <std/common.h>

//allows kernel stack in TSS to be changed
void set_kernel_stack(uint32_t stack);

//structure contains value of one GDT entry
//use attribute 'packed' to tell GCC not to change
//any of the alignment in the structure
struct gdt_entry_struct {
	uint16_t limit_low;	//lower 16 bits of limit
	uint16_t base_low;	//lower 16 bits of base
	uint8_t base_middle;	//next 8 bits of the base
	uint8_t access;		//access flags, determining ring for this segment to be used in
	uint8_t granularity;
	uint8_t base_high;	//last 8 bits of base
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct {
	uint16_t limit;		//upper 16 bits of all selector limits
	uint32_t base;		//address of the first gdt_entry_t struct
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

//publicly accessible initialization function
void gdt_install();
void idt_install();

//struct describing interrupt gate
struct idt_entry_struct {
	uint16_t base_lo;		//lower 16 bits of the address to jump to when this interrupt fires
	uint16_t sel;		//kernel segment selector
	uint8_t always0;		//must always be zero
	uint8_t flags;		//more flags
	uint16_t base_hi;		//upper 16 bits of address to jump to
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

//struct describing pointer to an array of interrupt handlers
//in a format suitable to be passed to 'lidt'
struct idt_ptr_struct {
	uint16_t limit;
	uint32_t base;		//address of the first element in our idt_entry_t array
} __attribute__((packed));
typedef struct idt_ptr_struct idt_ptr_t;

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

//extern directives allow us to access the addresses of our ASM ISR handlers
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void irq16();
extern void irq17();

extern void isr128();
 
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
