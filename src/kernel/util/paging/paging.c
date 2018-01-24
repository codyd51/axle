#include "paging.h"
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/boot_info.h>
#include <kernel/address_space.h>

//defined in kheap
extern uint32_t placement_address;
extern heap_t* kheap;

page_directory_t* page_dir_kern() {
    Deprecated();
}

page_directory_t* page_dir_current() {
    Deprecated();
}

void *mmap(void *addr, uint32_t length, int UNUSED(flags), int UNUSED(fd), uint32_t UNUSED(offset)) {
    Deprecated();
    /*
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
    */
}

int munmap(void *UNUSED(addr), uint32_t UNUSED(length)) {
    NotImplemented();
}

void* unsbrk(int UNUSED(increment)) {
    Deprecated();
	task_t* current = task_with_pid(getpid());
	char* brk = (char*)current->prog_break;
	return brk;
}

void* sbrk(int increment) {
    Deprecated();
	if (increment < 0) {
		ASSERT(0, "sbrk w/ neg increment");
		return unsbrk(increment);
	}

	task_t* current = task_with_pid(getpid());
	char* brk = (char*)current->prog_break;

	if (!increment) {
		return brk;
	}

	current->prog_break += increment;


	memset(brk, 0, increment);

	//map this new memory
	//mmap(brk, increment, 0, 0, 0);

	return brk;
}

int brk(void* addr) {
    Deprecated();
	printf("BRK(%x)\n", addr);
	task_t* current = task_with_pid(getpid());
	current->prog_break = (uint32_t)addr;
	return 0;
}

page_t* get_page(uint32_t address, int make, page_directory_t* dir) {
    Deprecated();
    return NULL;
}

static page_table_t* clone_table(page_table_t* src, uint32_t* physAddr) {
    Deprecated();
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
    Deprecated();
    /*
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
    */
}

void free_directory(page_directory_t* dir) {
    Deprecated();
    /*
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
    */
}

//stubs for deprecated methods
bool alloc_frame(page_t* page, int is_kernel, int is_writable) {
    Deprecated();
}

void free_frame(page_t* page) {
    Deprecated();
}
