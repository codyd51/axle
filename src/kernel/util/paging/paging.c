#include "paging.h"
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <kernel/util/multitasking/tasks/task.h>

//bitset of frames - used or free
uint32_t* frames;
uint32_t nframes;

page_directory_t* kernel_directory = 0;
page_directory_t* current_directory = 0;

volatile uint32_t memsize = 0; //size of paging memory

//defined in kheap
extern uint32_t placement_address;
extern heap_t* kheap;

//macros used in bitset algorithms
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

uint32_t get_cr0() {
	uint32_t cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	return cr0;
}

void set_cr0(uint32_t cr0) {
	asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

page_directory_t* get_cr3() {
	uint32_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	return (page_directory_t*)(long)cr3;
}

void set_cr3(page_directory_t* dir) {
	//uint32_t addr = (uint32_t)&dir->tables[0];
	//asm volatile("movl %%eax, %%cr3" :: "a" (addr));
	/*
	int cr0 = get_cr0();
	cr0 &= ~0x80000000;
	set_cr0(cr0);
	*/
	
	asm volatile("mov %0, %%cr3" : : "r"(dir->physicalAddr));
	//turn off paging first
	//asm volatile("mov %0, %%cr3":: "r"(dir->physicalAddr));
	//if paging is not already enabled
	int cr0 = get_cr0();
	//if (!(cr0 & 0x80000000)) {
		cr0 |= 0x80000000; //enable paging
		set_cr0(cr0);
	//}
}

//static function to set a bit in frames bitset
static void set_bit_frame(uint32_t frame_addr) {
	if (frame_addr < nframes * 4 * 0x400) {
		uint32_t frame = frame_addr/PAGE_SIZE;
		uint32_t idx = INDEX_FROM_BIT(frame);
		uint32_t off = OFFSET_FROM_BIT(frame);
		frames[idx] |= (0x1 << off);
	}
	else {
		printk_err("set_bit_frame() couldn't set frame %x", frame_addr);
	}
}

//static function to clear a bit in the frames bitset
static void clear_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr/PAGE_SIZE;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[idx] &= ~(0x1 << off);
}

//static function to test if a bit is sset
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static uint32_t test_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr/PAGE_SIZE;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	return (frames[idx] & (0x1 << off));
}
#pragma GCC diagnostic pop

uint32_t* page_from_frame(int32_t frame) {
	int addr = frame * PAGE_SIZE;
	return (uint32_t*)(long)addr;
}

//static function to find the first free frame
static int32_t first_frame() {
	for (uint32_t i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		if (frames[i] != 0xFFFFFFFF) {
			//at least one free bit
			for (uint32_t j = 0; j < 32; j++) {
				uint32_t bit = 0x1 << j;
				if (!(frames[i] & bit)) {
					//found unused bit i in addr
					return i*4*8+j;
				}
			}
		}
	}
	printf_info("first_frame(): no free frames!");
	return -1;
}

void virtual_map_pages(long addr, unsigned long size, uint32_t rw, uint32_t user) {
	unsigned long i = addr;
	while (i < (addr + size + PAGE_SIZE)) {
		if (i + size < memsize) {
			//find first free frame
			set_bit_frame(first_frame());

			//set space to taken anyway
			kmalloc(PAGE_SIZE);
		}

		page_t* page = get_page(i, 1, current_directory);
		page->present = 1;
		page->rw = rw;
		page->user = user;
		page->frame = i / PAGE_SIZE;
		i += PAGE_SIZE;
	}
	return;
}

void vmem_map(uint32_t virt, uint32_t physical) {
	uint16_t id = virt >> 22;
	for (int i = 0; i < PAGE_SIZE; i++) {
		page_t* page = get_page(virt+ (i * PAGE_SIZE), 1, current_directory);
		page->present = 1;
		page->rw = 1;
		page->user = 1;
		page->frame = (virt+ (i * PAGE_SIZE)) / PAGE_SIZE;
	}
	printf_info("Mapping %x (%x) -> %x", virt, id, physical);
}

page_directory_t* page_dir_kern() {
	return kernel_directory;
}

page_directory_t* page_dir_current() {
	return current_directory;
}

//just set present bit!
//don't actually allocate frame until page is accessed
bool alloc_frame_lazy(page_t* page, int is_kernel, int is_writeable) {
	if (page->frame != 0) {
		//frame was already allocated, return early
		printk_err("alloc_frame_lazy fail, frame %x taken", page->frame * PAGE_SIZE);
		return false;
	}
	page->present = 1; //mark as present
	page->rw = is_writeable; //should page be writable?
	page->user = !is_kernel; //should page be user mode?
	page->frame = 0;
	return true;
}

//function to allocate a frame
bool alloc_frame(page_t* page, int is_kernel, int is_writeable) {
	if (page->frame != 0) {
		//frame was already allocated, return early
		printk_err("alloc_frame fail, frame %x taken", page->frame * PAGE_SIZE);
		return false;
	}
	
	int32_t idx = first_frame(); //index of first free frame
	if (idx == -1) {
		PANIC("No free frames!");
	}

	set_bit_frame(idx*PAGE_SIZE); //frame is now ours
	page->present = 1; //mark as present
	page->rw = is_writeable; //should page be writable?
	page->user = !is_kernel; //should page be user mode?
	page->frame = idx;

	return true;
}

//function to dealloc a frame
void free_frame(page_t* page) {
	uint32_t frame;
	if (!(frame = page->frame)) {
		//page didn't actually have an allocated frame!
		return;
	}
	clear_frame(frame); //frame is now free again
	page->frame = 0x0; //page now doesn't have a frame
	page->present = 0;
}

#define VESA_WIDTH 1024
#define VESA_HEIGHT 768
#define VESA_BPP 3
void identity_map_lfb(uint32_t location) { 
	uint32_t j = location;
	//TODO use screen object instead of these vals
	while (j < location + (VESA_WIDTH * VESA_HEIGHT * VESA_BPP)) {
		//if frame is valid
		if (j + location + (VESA_WIDTH * VESA_HEIGHT * VESA_BPP) < memsize) {
			set_bit_frame(j); //tell frame bitset this frame is in use
		}
		//get page
		page_t* page = get_page(j, 1, kernel_directory);
		//fill it
		page->present = 1;
		page->rw = 1;
		page->user = 1;
		page->frame = j / PAGE_SIZE;
		j += PAGE_SIZE;
	}
}

static void page_fault(registers_t regs);

void set_paging_bit(bool enabled) {
	kernel_begin_critical();
	if (enabled) {
		set_cr0(get_cr0() | 0x80000000);
	}
	else {
		set_cr0(get_cr0() & 0x80000000);
	}
	kernel_end_critical();
}

void force_frame(page_t *page, int is_kernel, int is_writeable, unsigned int addr) {
	page->present = 1;
	page->rw = is_writeable;
	page->user = !is_kernel;
	page->frame = addr / PAGE_SIZE;
	set_bit_frame(addr);
}

void paging_install() {
	printf_info("Initializing paging...");

	//size of physical memory
	//assume 256MB 
	uint32_t mem_end_page = 0x10000000;
	//uint32_t mem_end_page = memory_size;
	memsize = mem_end_page;

	nframes = mem_end_page / PAGE_SIZE;
	frames = (uint32_t*)kmalloc(INDEX_FROM_BIT(nframes) * sizeof(uint32_t));
	memset(frames, 0, INDEX_FROM_BIT(nframes));

	//make page directory
	// uint32_t phys;
	kernel_directory = (page_directory_t*)kmalloc_a(sizeof(page_directory_t));
	memset(kernel_directory, 0, sizeof(page_directory_t));
	kernel_directory->physicalAddr = (uint32_t)kernel_directory->tablesPhysical;

	//map pages in kernel heap area
	//we call get_page but not alloc_frame
	//this causes page_table_t's to be created where necessary
	//don't alloc the frames yet, they need to be identity
	//mapped below first.
    uint32_t i = 0;
	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += PAGE_SIZE) {
		get_page(i, 1, kernel_directory);
	}

	//we need to identity map (phys addr = virtual addr) from
	//0x0 to end of used memory, so we can access this
	//transparently, as if paging wasn't enabled
	//note, inside this loop body we actually change placement_address
	//by calling kmalloc(). A while loop causes this to be computed
	//on-the-fly instead of once at the start
	unsigned idx = 0;
	while (idx < (placement_address + PAGE_SIZE)) {
		//kernel code is readable but not writeable from userspace
		alloc_frame(get_page(idx, 1, kernel_directory), 1, 0);
		idx += PAGE_SIZE;
	}
	printf_info("Kernel VirtMem identity mapped up to %x", placement_address);


	//allocate pages we mapped earlier
	uint32_t heap_end = KHEAP_START + KHEAP_INITIAL_SIZE;
	for (i = KHEAP_START; i < heap_end; i += PAGE_SIZE) {
		page_t* page = get_page(i, 1, kernel_directory);
		alloc_frame(page, 1, 0);
		unsigned char* ptr = (unsigned char*)kernel_directory->tables;
		if (*ptr == 0xff) {
			printf("broken! %x %x %x\n", KHEAP_START, KHEAP_INITIAL_SIZE, heap_end);
		}
	}

	//before we enable paging, register page fault handler
	register_interrupt_handler(14, page_fault);

	//enable paging
	switch_page_directory(kernel_directory);
	//turn on paging
	set_paging_bit(true);

	//initialize kernel heap
	kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, KHEAP_MAX_ADDRESS, 0, 0);
	//move_stack((void*)0xE0000000, 0x2000);
	//expand(0x1000000, kheap);

	current_directory = clone_directory(kernel_directory);
	switch_page_directory(current_directory);	

	page_regions_print(current_directory);
}

void page_regions_print(page_directory_t* dir) {
	if (!dir) return;
	printk("page directory %x regions:\n", dir);

	int32_t run_start = -1;
	for (int i = 0; i < 1024; i++) {
		page_table_t* tab = dir->tables[i];
		if (!tab) continue;

		for (int j = 0; j < 1024; j++) {
			if (tab->pages[j].present) {
				//page present
				//start run if we're not in one
				if (run_start == -1) {
					run_start = tab->pages[j].frame * PAGE_SIZE;
				}
			}
			else {
				//are we in a run?
				if (run_start != -1) {
					//run finished!
					//run ends on previous page
					uint32_t run_end = (tab->pages[j-1].frame * PAGE_SIZE);
					printk("[%x - %x]\n", run_start, run_end);

					//reset run state
					run_start = -1;
				}
			}
		}
	}
}

void *mmap(void *addr, uint32_t length, int UNUSED(flags), int UNUSED(fd), uint32_t UNUSED(offset)) {
	char* chbuf = (char*)addr;
	int diff = (uintptr_t)chbuf % PAGE_SIZE;
	if (diff) {
		printf("mmap page-aligning from %x to %x\n", (uint32_t)addr, (uint32_t)addr - diff);
		length += diff;
		chbuf -= diff;
	}
	uint32_t page_aligned = length;
	if (page_aligned % PAGE_SIZE) {
		page_aligned = length + 
					   (PAGE_SIZE - (length % PAGE_SIZE));
		printf("mmap page-aligning chunk size from %x to %x\n", length, page_aligned);
	}

	printf("mmap @ %x + %x\n", addr, page_aligned);
	//allocate every necessary page
	for (uint32_t i = 0; i < page_aligned; i += PAGE_SIZE) {
		//TODO change alloc_frame flags based on 'flags'
		alloc_frame(get_page((uint32_t)chbuf + i, 1, current_directory), 1, 1);
		memset((uint32_t*)chbuf + i, 0, PAGE_SIZE);
	}
	return chbuf;
}

int munmap(void *UNUSED(addr), uint32_t UNUSED(length)) {
	ASSERT(0, "munmap called");
}

void* unsbrk(int UNUSED(increment)) {
	task_t* current = task_with_pid(getpid());
	char* brk = (char*)current->prog_break;
	return brk;
}

void* sbrk(int increment) {
	if (increment < 0) {
		ASSERT(0, "sbrk w/ neg increment");
		return unsbrk(increment);
	}

	task_t* current = task_with_pid(getpid());
	char* brk = (char*)current->prog_break;

	if (!increment) {
		return brk;
	}

	//printf("sbrk %x + %x\n", current->prog_break, increment);
	current->prog_break += increment;

	/*
	int page_count = increment / 0x1000;
	if (increment % 0x1000) page_count++;
	for (int i = 0; i < page_count; i++) {
		page_t* new = get_page(brk + (i * 0x100), 1, current->page_dir);
		alloc_frame(new, 0, 1);
	}
	*/

	memset(brk, 0, increment);

	//map this new memory
	//mmap(brk, increment, 0, 0, 0);

	return brk;
}

int brk(void* addr) {
	printf("BRK(%x)\n", addr);
	task_t* current = task_with_pid(getpid());
	current->prog_break = (uint32_t)addr;
	return 0;
}

void switch_page_directory(page_directory_t* dir) {
	current_directory = dir;
	set_cr3(dir);
}

uint32_t vmem_from_frame(uint32_t phys) {
	phys *= PAGE_SIZE;
	return phys;
}

page_t* get_page(uint32_t address, int make, page_directory_t* dir) {
	//turn address into an index
	address /= PAGE_SIZE;
	//find page table containing this address
	uint32_t table_idx = address / 1024;

	ASSERT(table_idx < 1024, "get_page called with unreasonable address %x\n", address);

	//if this page is already assigned
	if (dir->tables[table_idx]) {
		return &dir->tables[table_idx]->pages[address%1024];
	}
	else if (make) {
		uint32_t tmp;
		dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
		memset(dir->tables[table_idx], 0, sizeof(page_table_t));
		//printf("page_table_t alloc %d %x\n", table_idx, dir->tables[table_idx]);
		//PRESENT, RW, US
		dir->tablesPhysical[table_idx] = tmp | 0x7;
		return &dir->tables[table_idx]->pages[address%1024];
	}
	return 0;
}

void page_fault(registers_t regs) {
	//page fault has occured
	//faulting address is stored in CR2 register
	uint32_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	//error code tells us what happened
	int present = !(regs.err_code & 0x1); //page not present
	int rw = regs.err_code & 0x2; //write operation?
	int us = regs.err_code & 0x4; //were we in user mode?
	int reserved = regs.err_code & 0x8; //overwritten CPU-reserved bits of page entry?
	int id = regs.err_code & 0x10; //caused by instruction fetch?

	//if this page was present, attempt to recover by allocating the page
	if (present) {
		//bool attempt = alloc_frame(get_page(faulting_address, 1, current_directory), 1, 1);
		//bool attempt = false;
		if (0) {
			//recovered successfully
			//printf_info("allocated page at virt %x", faulting_address);
			return;
		}
	}

	//if execution reaches here, recovery failed or recovery wasn't possible
	printf_err("page fault @ virt %x, flags: ", faulting_address);
	printf_err("%spresent", present ? "" : "not ");
	printf_err("%s operation", rw ? "write" : "read");
	printf_err("%s mode", us ? "user" : "kernel");

	if (reserved) printf_err("Overwrote CPU-resereved bits of page entry");
	if (id) printf_err("Faulted during instruction fetch");

	bool caused_by_execution = (regs.eip == faulting_address);
	printf_err("caused by %s unpaged memory", caused_by_execution ? "executing" : "reading");

	extern void common_halt(registers_t regs, bool recoverable);
	common_halt(regs, false);
}

static page_table_t* clone_table(page_table_t* src, uint32_t* physAddr) {
	printk("cloning table at %x\n", src);
	int cloned_pages = 0;

	//make new page aligned table
	page_table_t* table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), physAddr);
	//ensure new table is blank
	memset((uint8_t*)table, 0, sizeof(page_table_t));

	//for each entry in table
	for (int i = 0; i < 1024; i++) {
		//if source entry has a frame associated with it
		if (!src->pages[i].frame) continue;
		cloned_pages++;

		//get new frame
		alloc_frame(&(table->pages[i]), 0, 0);
		//clone flags from source to destination
		table->pages[i].present = src->pages[i].present;
		table->pages[i].rw = src->pages[i].rw;
		table->pages[i].user = src->pages[i].user;
		table->pages[i].accessed = src->pages[i].accessed;
		table->pages[i].dirty = src->pages[i].dirty;

		//physically copy data across
		extern void copy_page_physical(uint32_t page, uint32_t dest);
		copy_page_physical(src->pages[i].frame * PAGE_SIZE, table->pages[i].frame * PAGE_SIZE);
	}
	printk("clone_table() copied %d pages, %d kb\n", cloned_pages, ((cloned_pages * PAGE_SIZE) / 1024));
	return table;
}

page_directory_t* clone_directory(page_directory_t* src) {
	printk_info("cloning page directory at phys %x virt %x", src->physicalAddr, src);

	uint32_t phys;

	//make new page directory and get physaddr
	//heap_print(-1);
	page_directory_t* dir = (page_directory_t*)kmalloc_ap(sizeof(page_directory_t), &phys);

	//blank it
	memset((uint8_t*)dir, 0, sizeof(page_directory_t));

	//get offset of tablesPhysical from start of page_directory_t
	uint32_t offset = (uint32_t)dir->tablesPhysical - (uint32_t)dir;
	dir->physicalAddr = phys + offset;

	//for each page table
	//if in kernel directory, don't make copy
	for (int i = 0; i < 1024; i++) {
		if (!src->tables[i]) continue;

		if (kernel_directory->tables[i] == src->tables[i]) {
			//in kernel, so just reuse same pointer (link)
			dir->tables[i] = src->tables[i];
			dir->tablesPhysical[i] = src->tablesPhysical[i];
		}
		else {
			//copy table
			uint32_t phys;
			dir->tables[i] = clone_table(src->tables[i], &phys);
			printk("cloned table: %x\n", dir->tables[i]);
			dir->tablesPhysical[i] = phys | 0x07;
		}
	}
	return dir;
}

void free_directory(page_directory_t* dir) {
	//first free all tables
	for (int i = 0; i < 1024; i++) {
		if (!dir->tables[i]) {
			continue;
		}

		page_table_t* table = dir->tables[i];
		//only free pages in table if table wasn't linked in from kernel tables
		if (kernel_directory->tables[i] == table) {
			//printf("free_directory() page table %x was linked from kernel\n", table);
			continue;
		}
		//printf("free_directory() proc owned table %x\n", table);

		//this page table belonged to the dead process alone
		//free pages in table
		for (int j = 0; j < 1024; j++) {
			page_t page = table->pages[j];
			free_frame(&page);
		}

		//free table itself
		kfree(table);
	}
	//finally, free directory
	kfree(dir);
}

