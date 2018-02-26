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
}

void vmm_copy_page_table_pointers(vmm_pdir_t* src, vmm_pdir_t* dst) {
    for (int i = 0; i < 1024; i++) {
        if (!src->tablesPhysical[i]) {
            continue;
        }
        if (dst->tablesPhysical[i]) {
            panic("tried to overwrite pde entry in dst");
        }
        printf("vmm_copy_page_table_pointers copying page table pointer #%d\n", i);
        dst->tablesPhysical[i] = src->tablesPhysical[i];
    }
}

vmm_pdir_t* vmm_setup_new_pdir() {
    uint32_t phys;
    vmm_pdir_t* new_dir = kmalloc_ap(sizeof(vmm_pdir_t), &phys);
    memset(new_dir, 0, sizeof(vmm_pdir_t));
    phys += offsetof(vmm_pdir_t, tablesPhysical);
    new_dir->physicalAddr = phys;
    printf("VMM new dir at 0x%08x, physicalAddr 0x%08x\n", new_dir, phys);

    //map in kernel code + data
    //these mappings are shared between all page directories
    vmm_pdir_t* kernel_pdir = boot_info_get()->vmm_kernel;
    vmm_copy_page_table_pointers(kernel_pdir, new_dir);
    //set up recursive page table entry
    //TODO(PT): put this into its own function, its done here and in vmm_init
    new_dir->tablesPhysical[1023] = new_dir->physicalAddr | 0x7;

    return new_dir;
}

vmm_pdir_t* vmm_clone_active_page_table_at_index(vmm_pdir_t* dst_dir, uint32_t table_index) {
    printf("vmm_clone_active_page_table_at_index %d\n", table_index);
    unsigned long* src_table = ((unsigned long *)0xFFC00000) + (0x400 * table_index);

	//make new page aligned table
    uint32_t physAddr;
	unsigned long* new_table = (unsigned long*)kmalloc_ap(sizeof(page_table_t), (uint32_t*)&physAddr);
	//ensure new table is blank
	memset((uint8_t*)new_table, 0, sizeof(page_table_t));
    dst_dir->tablesPhysical[table_index] = physAddr | 0x07;

	//for each entry in table
	int cloned_pages = 0;
	for (int page_idx = 0; page_idx < 1024; page_idx++) {
        if (!src_table[page_idx]) {
            continue;
        }
        printf("contents of page %d = 0x%08x\n", src_table[page_idx]);
		cloned_pages++;

        printf("is this the same? 0x%08x 0x%08x\n", (src_table[page_idx] >> 22) * PAGING_FRAME_SIZE, src_table[page_idx]);

        uint32_t source_frame = (src_table[page_idx] >> 22) * PAGING_FRAME_SIZE;
        uint32_t dest_frame = pmm_alloc();

        new_table[page_idx] = dest_frame / PAGING_FRAME_SIZE;
        //clone the flags from the source
        //present, rw, user, accessed, dirty = 0b11111 = 0x1f
        new_table[page_idx] |= src_table[page_idx] & 0x1f;

		//physically copy data across
		extern void copy_page_physical(uint32_t page, uint32_t dest);
        copy_page_physical(source_frame, dest_frame);
	}
	printf("clone_table() copied %d pages, %d kb\n", cloned_pages, ((cloned_pages * PAGE_SIZE) / 1024));
}

vmm_pdir_t* vmm_clone_active_pdir() {
    vmm_pdir_t* src = vmm_active_pdir();
    printf("VMM cloning page directory 0x%08x\n", src);
    vmm_pdir_t* new_dir = vmm_setup_new_pdir();

    //copy each table
    for (int i = 0; i < 1024; i++) {
        //no table to copy at this index from the source
        if (!src->tablesPhysical[i]) {
            continue;
        }
        //if there's already a kernel mapping for this table, skip it
        //TODO(PT) this will waste tables that only have a couple kernel pages
        //in use! do we care?
        if (new_dir->tablesPhysical[i]) {
            printf("vmm_clone_pdir not copying already copied table index %d\n", i);
            continue;
        }

        //copy table
        uint32_t table_phys;
        unsigned long pdindex = i;
        unsigned long * pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);

        vmm_clone_active_page_table_at_index(new_dir, i);
        new_dir->tables[i] = clone_table(pt, &table_phys);
        printf("cloned table: 0x%08x phys 0x%08x\n", pt, table_phys);
        new_dir->tablesPhysical[i] = table_phys | 0x07;
    }

    return new_dir;
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
