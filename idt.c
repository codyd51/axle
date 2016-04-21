#include <system.h>

//IDT entry
struct idt_entry {
	unsigned short base_lo;
	//kernel segment goes here
	unsigned short sel;
	//this is ALWAYS 0
	unsigned char always0;
	unsigned char flags;
	unsigned short base_hi;
} __attribute__((packed));

struct idt_ptr {
	unsigned short limit;
	unsigned int base;
} __attribute__((packed));

//declare IDT of 256 entries. If an undefined IDT entry is iht, it'll normally
//cause an unhandled interrupt exception. Any descriptor for which the 'presence'
//bit is cleared will generate an unhandled interrupt exception
struct idt_entry idt[256];
struct idt_ptr idtp;

//exists in asm file to load IDT
extern void idt_load();