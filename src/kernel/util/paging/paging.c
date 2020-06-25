#include "paging.h"
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <kernel/multitasking/tasks/task.h>
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

vmm_page_directory_t* vmm_clone_active_page_table_at_index(vmm_page_directory_t* dst_dir, uint32_t table_index) {
	Deprecated();
}

page_directory_t* clone_directory(page_directory_t* src) {
    Deprecated();
}

void free_directory(page_directory_t* dir) {
    Deprecated();
}

//stubs for deprecated methods
bool alloc_frame(vmm_page_t* page, int is_kernel, int is_writable) {
    Deprecated();
}

void free_frame(vmm_page_t* page) {
    Deprecated();
}

vmm_page_t* get_page(uint32_t address, int make, page_directory_t* dir) {
	Deprecated();
}