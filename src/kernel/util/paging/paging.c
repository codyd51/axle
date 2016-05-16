#include "paging.h"
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>

//bitset of frames - used or free
u32int* frames;
u32int nframes;

page_directory_t* kernel_directory;
page_directory_t* current_directory;

//defined in kheap
extern u32int placement_address;
extern heap_t* kheap;

//macros used in bitset algorithms
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

//static function to set a bit in frames bitset
static void set_frame(u32int frame_addr) {
	u32int frame = frame_addr/0x1000;
	u32int idx = INDEX_FROM_BIT(frame);
	u32int off = OFFSET_FROM_BIT(frame);
	frames[idx] |= (0x1 << off);
}

//static function to clear a bit in the frames bitset
static void clear_frame(u32int frame_addr) {
	u32int frame = frame_addr/0x1000;
	u32int idx = INDEX_FROM_BIT(frame);
	u32int off = OFFSET_FROM_BIT(frame);
	frames[idx] &= ~(0x1 << off);
}

//static function to test if a bit is sset
static u32int test_frame(u32int frame_addr) {
	u32int frame = frame_addr/0x1000;
	u32int idx = INDEX_FROM_BIT(frame);
	u32int off = OFFSET_FROM_BIT(frame);
	return (frames[idx] & (0x1 << off));
}

//static function to find the first free frame 
static u32int first_frame() {
	u32int i, j;
	for (int i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		if (frames[i] != 0xFFFFFFFF) {
			//nothing free, exit early
			for (j = 0; j < 32; j++) {
				u32int toTest = 0x1 << j;
				if (!(frames[i] & toTest)) {
					return i*4*8+j;
				}
			}
		}
	}
}

//function to allocate a frame
void alloc_frame(page_t* page, int is_kernel, int is_writeable) {
	if (page->frame != 0) {
		//frame was already allocated, return early
		return;
	}
	u32int idx = first_frame(); //index of first free frame
	if (idx == (u32int)-1) {
		PANIC("No free frames!");
	}
	set_frame(idx*0x1000); //frame is now ours
	page->present = 1; //mark as present
	page->rw = (is_writeable) ? 1 : 0; //should page be writable?
	page->user = (is_kernel) ? 0 : 1; //should page be user mode?
	page->frame = idx;
}

//function to dealloc a frame
void free_frame(page_t* page) {
	u32int frame;
	if (!(frame=page->frame)) {
		//page didn't actually have an allocated frame!
		return;
	}
	clear_frame(frame); //frame is now free again
	page->frame = 0x0; //page now doesn't have a frame
}

void initialize_paging() {
	//size of physical memory
	//assume 16MB
	
	u32int mem_end_page = 0x10000000;
		
	printf_dbg("mem_end_page");

	nframes = mem_end_page / 0x1000;
	frames = (u32int*)kmalloc(INDEX_FROM_BIT(nframes));
	memset(frames, 0, INDEX_FROM_BIT(nframes));

	printf_dbg("memset");

	//make page directory
	kernel_directory = (page_directory_t*)kmalloc_a(sizeof(page_directory_t));
	memset(kernel_directory, 0, sizeof(page_directory_t));
	current_directory = kernel_directory;

	printf_dbg("current_directory");

	//map pages in kernel heap area
	//we call get_page but not alloc_frame
	//this causes page_table_t's to be created where necessary
	//don't alloc the frames yet, they need to be identity 
	//mapped below first.
	int i = 0;
	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += 0x1000) {
		get_page(i, 1, kernel_directory);
	}
	printf_dbg("map kernel pages");

	//we need to identity map (phys addr = virtual addr) from
	//0x0 to end of used memory, so we can access this
	//transparently, as if paging wasn't enabled
	//note, inside this loop body we actually change placement_address
	//by calling kmalloc(). A while loop causes this to be computed
	//on-the-fly instead of once at the start
	int idx = 0;
	while (idx < placement_address + 0x1000) {
		//kernel code is readable but not writeable from userspace
		alloc_frame(get_page(idx, 1, kernel_directory), 0, 0);
		idx += 0x1000;
	}
	printf_dbg("identity map");

	//allocate pages we mapped earlier
	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += 0x1000) {
		alloc_frame(get_page(i, 1, kernel_directory), 0, 0);
	}

	//before we enable paging, register page fault handler
	register_interrupt_handler(14, page_fault);

	printf_dbg("register handler");

	//enable paging
	switch_page_directory(kernel_directory);

	printf_dbg("switch_page_directory");

	//turn on paging
	asm volatile("cli");
	u32int cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= 0x80000000;
	asm volatile("mov %0, %%cr0" : : "r"(cr0));
	asm volatile("sti");

	printf_dbg("paging enabled");

	//initialize kernel heap
	kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, 0xCFFFF000, 0, 0);
}

void switch_page_directory(page_directory_t* dir) {
	current_directory = dir;
	asm volatile("mov %0, %%cr3" : : "r"(dir->tablesPhysical));
}

page_t* get_page(u32int address, int make, page_directory_t* dir) {
	//turn address into an index
	address /= 0x1000;
	//find page table containing this address
	u32int table_idx = address / 1024;

	//if this page is already assigned
	if (dir->tables[table_idx]) {
		return &dir->tables[table_idx]->pages[address%1024];
	}
	else if (make) {
		u32int tmp;
		dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
		memset(dir->tables[table_idx], 0, 0x1000);
		//PRESENT, RW, US
		dir->tablesPhysical[table_idx] = tmp | 0x7;
		return &dir->tables[table_idx]->pages[address%1024];
	}
	return 0;
}

void page_fault(registers_t regs) {
	//page fault has occured
	//faulting address is stored in CR2 register
	u32int faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	//error code tells us what happened
	int present = !(regs.err_code & 0x1); //page not present
	int rw = regs.err_code & 0x2; //write operation?
	int us = regs.err_code & 0x4; //were we in user mode?
	int reserved = regs.err_code & 0x8; //overwritten CPU-reserved bits of page entry?
	int id = regs.err_code & 0x10; //caused by instruction fetch?

	//output error message
	printf("Page fault! ( ");
	if (present) printf("present ");
	if (rw) printf("read-only ");
	if (us) printf("user-mode ");
	if (reserved) printf("reserved ");
	printf(") at 0x%x\n", faulting_address);
	PANIC("Page fault");
}

/*

#define PANIC(x) panic(x, __LINE__, __FILE__);

void panic(const char* err, u32int line, const char* file) {
	printf("[%s - line %d]: %s", file, line, err);
	//enter infinite loop
	do {} while (1);
}
void page_fault(registers_t regs) {
	//page fault has occured
	//faulting address is stored in CR2 register
	u32int faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	//error code tells us what happened
	int present = !(regs.err_code & 0x1); //page not present
	int rw = regs.err_code & 0x2; //write operation?
	int us = regs.err_code & 0x4; //were we in user mode?
	int reserved = regs.err_code & 0x8; //overwritten CPU-reserved bits of page entry?
	int id = regs.err_code & 0x10; //caused by instruction fetch?

	//output error message
	printf("Page fault! ( ");
	if (present) printf("present ");
	if (rw) printf("read-only ");
	if (us) printf("user-mode ");
	if (reserved) printf("reserved ");
	printf(") at 0x%x\n", faulting_address);
	PANIC("Page fault");
}

unsigned long* page_directory;
unsigned long* page_table;

void map(unsigned int frame, unsigned int vframe) {
	page_table[frame] = vframe*4096 | 3; //attribute set to: kernel level, r/w, present (011)
}

void init_page_table() {
	//holds physical address of where a page is located
	unsigned long address = 0;
	unsigned int i;

	//map first 4MB of memory
	for (i = 0; i < 1024; i++) {
		page_table[i] = address | 3; //attribute set to: kernel level, r/w, present (011)
		address = address + 4096; //4096 = 4kb
	}
}

void init_page_directory() {
	//fill in first entry of page directory
	page_directory[0] = (unsigned long)page_table; //attribute set to: kernel level, r/w, present (011)
	page_directory[0] = page_directory[0] | 3;

	//fill in rest of page table
	for (int i = 1; i < 1024; i++) {
		page_directory[i] = 0 | 2; //attribute set to: kernel level, r/w, not present (010);
	}
}

void initialize_paging() {
	page_directory = (unsigned long*)kmalloc_a(4096);
	page_table = (unsigned long*)kmalloc_a(4096);

	unsigned int i;
	for (int i = 0; i < 1024; i++) {
		map(i, i);
	}

	init_page_directory();

	write_cr3(page_directory); //put page directory addr into cr3


	write_cr0(read_cr0() | 0x80000000); //set paging bit in cr0 to 1

	
	register_interrupt_handler(14, page_fault);
/*
	u32int cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= 0x80000000;
	asm volatile("mov %0, %%cr0" : : "r"(cr0));
*
	terminal_settextcolor(COLOR_MAGENTA);
	printf("Paging enabled.\n");
}

*/
