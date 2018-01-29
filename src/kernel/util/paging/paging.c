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

void vmm_copy_page_table_pointers(vmm_pdir_t* src, vmm_pdir_t* dst) {
    for (int i = 0; i < 1024; i++) {
        if (!src->tablesPhysical[i]) {
            continue;
        }
        if (dst->tablesPhysical[i]) {
            panic("tried to overwrite pde entry in dst");
        }
        printf("vmm_copy_page_table_pointers copying %d\n", i);
        dst->tablesPhysical[i] = src->tablesPhysical[i];
    }
}

vmm_pdir_t* vmm_clone_pdir(vmm_pdir_t* src) {
    printf("VMM cloning page directory 0x%08x\n", src);
    uint32_t phys;
    vmm_pdir_t* new_dir = kmalloc_ap(sizeof(vmm_pdir_t), &phys);
    memset(new_dir, 0, sizeof(vmm_pdir_t));
    uint32_t tablesOffset = (uint32_t)src->tablesPhysical - (uint32_t)src;
    phys += tablesOffset;
    new_dir->physicalAddr = phys;
    printf("VMM new dir at 0x%08x, physicalAddr 0x%08x\n", new_dir, phys);

    //map in kernel code + data
    //these mappings are shared between all page directories
    vmm_pdir_t* kernel_pdir = boot_info_get()->vmm_kernel;
    vmm_copy_page_table_pointers(kernel_pdir, new_dir);
    //set up recursive page table entry
    //TODO(PT): put this into its own function, its done here and in vmm_init
    new_dir->tablesPhysical[1023] = new_dir->physicalAddr | 0x7;

    //copy each table
    for (int i = 0; i < 1024; i++) {
        if (!src->tablesPhysical[i]) {
            continue;
        }
        //if there's already a kernel mapping for this table, skip it
        //TODO(PT) this will waste tables that only have a couple kernel pages
        //in use! do we care?
        if (new_dir->tablesPhysical[i]) {
            continue;
        }

        /*
        //copy table
        uint32_t table_phys;
        dir->tablesPhysical[i] = vmm_clone_table(src->tables[i]);
        dir->tables[i] = clone_table(src->tables[i], &phys);
        printk("cloned table: %x\n", dir->tables[i]);
        dir->tablesPhysical[i] = phys | 0x07;
        */
        printf("table %d in src and not in new_dir\n", i);
        panic("src has tables that aren't in kernel dir");
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
