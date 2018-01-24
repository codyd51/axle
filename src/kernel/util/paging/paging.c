#include "paging.h"
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/boot_info.h>
#include <kernel/address_space.h>

static void page_fault(register_state_t regs);

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
	asm volatile("mov %0, %%cr3" : : "r"(dir->physicalAddr));
	int cr0 = get_cr0();
	cr0 |= 0x80000000; //enable paging bit
	set_cr0(cr0);
}

//static function to set a bit in frames bitset
static void set_bit_frame(uint32_t frame_addr) {
    NotImplemented();
}

//static function to clear a bit in the frames bitset
static void clear_frame(uint32_t frame_addr) {
    NotImplemented();
}

uint32_t* page_from_frame(int32_t frame) {
    NotImplemented();
}

//static function to find the first free frame
static int32_t first_frame() {
    NotImplemented();
}

void virtual_map_pages(long addr, unsigned long size, uint32_t rw, uint32_t user) {
    NotImplemented();
}

void vmem_map(uint32_t virt, uint32_t physical) {
    NotImplemented();
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
    /*
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
    */
    NotImplemented();
}

//function to allocate a frame
bool alloc_frame(page_t* page, int is_kernel, int is_writeable) {
    /*
	if (page->frame != 0) {
		//frame was already allocated, return early
		printk_err("alloc_frame() page already assigned frame %x", page->frame * PAGE_SIZE);
		//return false;
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
    */
    NotImplemented();
}

//function to dealloc a frame
void free_frame(page_t* page) {
    /*
	uint32_t frame;
	if (!(frame = page->frame)) {
		//page didn't actually have an allocated frame!
		return;
	}
	clear_frame(frame); //frame is now free again
	page->frame = 0x0; //page now doesn't have a frame
	page->present = 0;
    */
    NotImplemented();
}

void identity_map_lfb(uint32_t location) {
    NotImplemented();
}

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
    NotImplemented();
}

#define PAGES_IN_PAGE_TABLE 1024
#define PAGE_TABLES_IN_PAGE_DIR 1024

static uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr) {
    uint32_t page_idx = addr / PAGING_PAGE_SIZE;
    uint32_t table_idx = page_idx / PAGE_TABLES_IN_PAGE_DIR;
    return table_idx;
}

static uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr) {
    uint32_t page_idx = addr / PAGING_PAGE_SIZE;
    return page_idx % PAGES_IN_PAGE_TABLE;
}

static void vmm_page_table_alloc_for_virt_addr(page_directory_t* dir, uint32_t addr) {
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(addr);
    //does this page table already exist?
    if (!dir->tables[table_idx]) {
        //create the page table
		uint32_t tmp;
		dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
		memset(dir->tables[table_idx], 0, sizeof(page_table_t));
		//PRESENT, RW, US
		dir->tablesPhysical[table_idx] = tmp | 0x7;
    }
}

static void create_heap_page_tables(page_directory_t* dir) {
	//here, we call get_page but not alloc_frame
	//this causes page_table_t's to be alloc'd if not already existing
	//we call this function before identity map so all page tables needed to alloc
	//heap pages will be mapped into the address space.
	for (uint32_t i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += PAGE_SIZE) {
        vmm_page_table_alloc_for_virt_addr(dir, i);
	}
}
        //this relies on a page table being a frame size, so verify this assumption
        assert(PAGING_FRAME_SIZE == sizeof(page_table_t), "page_table_t was a different size from a frame");
        //TODO (PT) add a check here for whether paging is active here
        //if it is, maybe it should be a virtual memory allocator?
        //high when i wrote this so feel free to discard if incorrect/wrong assumption`
        //1/23 high again, the above seems to be correct.
        //maybe a _vmm_internal_alloc() that either does pmm or vmm allocation,
        //based on whether paging is enabled.
        uint32_t identity_mapped_table_addr = pmm_alloc();
        page_table_t* identity_mapped_table = (page_table_t*)identity_mapped_table_addr;

		dir->tables[table_idx] = identity_mapped_table;
		//PRESENT, RW, US
        uint32_t table_page_flags = 0x7;
		dir->tablesPhysical[table_idx] = (identity_mapped_table_addr) | table_page_flags;

static void map_heap_pages(page_directory_t* dir) {
	//all of the page tables necessary have already been alloc'd and identity mapped thanks
	//to the loop just before the identity map
	uint32_t heap_end = KHEAP_START + KHEAP_INITIAL_SIZE;
	//figure out how much memory is being reserved for heap
	uint32_t heap_kb = (heap_end - KHEAP_START) / 1024;
	float heap_mb = heap_kb / 1024.0;
	printf_info("reserving %x MB for kernel heap", heap_mb);
	for (uint32_t i = KHEAP_START; i < heap_end; i += PAGE_SIZE) {
        vmm_page_alloc_for_virt_addr(dir, i);
	}
}

static void vmm_remap_kernel(page_directory_t* dir, uint32_t dest_virt_addr) {
    boot_info_t* info = boot_info_get();
    uint32_t kernel_base = info->kernel_image_start;
    uint32_t kernel_size = info->kernel_image_size;

    for (int i = 0; i < kernel_size; i += PAGING_PAGE_SIZE) {
        //figure out the address of the physical frame
        uint32_t frame_addr = i + kernel_base;

        //map this kernel frame to a virtual page
        uint32_t dest_page_addr = dest_virt_addr + i;
        page_t* dest_page = vmm_get_page_for_virtual_address(dir, dest_page_addr);
        vmm_map_page_to_frame(dest_page, frame_addr);
    }
}

void vmm_identity_map_region(page_directory_t* dir, uint32_t start, uint32_t size) {
    if (start & ~PAGING_FRAME_MASK) {
        panic("vmm_identity_map_region start not page aligned!");
    }
    if (size & ~PAGING_FRAME_MASK) {
        panic("vmm_identity_map_region size not page aligned!");
    }

    printf_dbg("Identity mapping from 0x%08x to 0x%08x", start, start + size);
    int frame_count = size / PAGING_PAGE_SIZE;
    for (int i = 0; i < frame_count; i++) {
        uint32_t page = start + (i * PAGING_PAGE_SIZE);
        vmm_page_alloc_for_phys_addr(dir, page);
    }
}

void paging_install() {
	printf_info("Initializing paging...");

    boot_info_t* info = boot_info_get();

	kernel_directory = (page_directory_t*)kmalloc_a(sizeof(page_directory_t));
	memset(kernel_directory, 0, sizeof(page_directory_t));
    //we know tablesPhysical is the physical address because paging isn't enabled yet
	kernel_directory->physicalAddr = (uint32_t)kernel_directory->tablesPhysical;

    /*
	//reference kernel heap page tables,
	//forcing them to be alloc'd before we identity map all alloc'd memory
	create_heap_page_tables(kernel_directory);

	//we need to identity map (phys addr = virtual addr) from
	//0x0 to end of used memory, so we can access this
	//transparently, as if paging wasn't enabled
	//note, inside this loop body we actually change placement_address
	//by calling kmalloc(). A while loop causes this to be computed
	//on-the-fly instead of once at the start
	unsigned idx = 0;
	while (idx < (placement_address + PAGE_SIZE)) {
		//kernel code is readable but not writeable from userspace
		//alloc_frame(get_page(idx, 1, kernel_directory), 1, 0);
        vmm_alloc_page(kernel_directory, idx);
		idx += PAGE_SIZE;
	}
	printf_info("Kernel VirtMem identity mapped up to %x", placement_address);
    */
    //map from 0x0 to top of kernel stack
    uint32_t identity_map_min = 0x0;
    uint32_t identity_map_max = info->kernel_image_end;

    if (identity_map_min & ~PAGING_FRAME_MASK) {
        panic("identity_map_max not page aligned!");
    }
    if (identity_map_max & ~PAGING_FRAME_MASK) {
        panic("identity_map_max not page aligned!");
    }

    printf_dbg("Identity mapping from 0x%08x to 0x%08x", identity_map_min, identity_map_max);
    for (int i = identity_map_min; i < identity_map_max; i += PAGING_PAGE_SIZE) {
        vmm_page_alloc_for_phys_addr(kernel_directory, i);
    }

    printf_dbg("Mapping kernel heap");
	//allocate initial heap pages
	map_heap_pages(kernel_directory);

	//before we enable paging, register page fault handler
	interrupt_setup_callback(INT_VECTOR_INT14, page_fault);

	//enable paging
	switch_page_directory(kernel_directory);
	//turn on paging
	set_paging_bit(true);

    /*
	//initialize kernel heap
	kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, KHEAP_MAX_ADDRESS, 0, 0);
	//move_stack((void*)0xE0000000, 0x2000);
	//expand(0x1000000, kheap);

	current_directory = clone_directory(kernel_directory);
	switch_page_directory(current_directory);

	page_regions_print(current_directory);
    */
}

void page_regions_print(page_directory_t* dir) {
	if (!dir) return;
	printf("page directory %x regions:\n", dir);

	uint32_t run_start, run_end;
	bool in_run = false;
	for (int i = 0; i < 1024; i++) {
		page_table_t* tab = dir->tables[i];
		if (!tab) continue;

		//page tables map 1024 4kb pages
		//page directories contains 1024 page tables
		//therefore, each page table maps 4mb of the virtual addr space
		//for a given table index and page index, the virtual address is:
		//table index * (range mapped by each table) + page index * (range mapped by each page)
		//table index * 4mb + page index * 4kb
		int page_table_virt_range = PAGE_SIZE * PAGE_SIZE / 4;

		for (int j = 0; j < 1024; j++) {
			if (tab->pages[j].present || tab->pages[j].frame) {
				//page present
				//start run if we're not in one
				if (!in_run) {
					in_run = true;
					run_start = (i * page_table_virt_range) + (j * PAGE_SIZE);
				}
			}
			else {
				//are we in a run?
				if (in_run) {
					//run finished!
					//run ends on previous page
					//uint32_t run_end = (tab->pages[j-1].frame * PAGE_SIZE);
					run_end = (i * page_table_virt_range) + (j * PAGE_SIZE);
					printf("[0x%08x - 0x%08x]\n", run_start, run_end);

					in_run = false;
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

page_t* vmm_get_page_for_virtual_address(page_directory_t* dir, uint32_t virt_addr) {
    if (virt_addr & ~PAGING_FRAME_MASK) {
        panic("vmm_page_for_virtual_address() frame address not page-aligned");
    }
    uint32_t page_in_table_idx = vmm_page_idx_within_table_for_virt_addr(virt_addr);
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(virt_addr);

    //does this page table already exist?
    if (!dir->tables[table_idx]) {
        vmm_page_table_alloc_for_virt_addr(dir, virt_addr);
    }
    page_t* page = &dir->tables[table_idx]->pages[page_in_table_idx];
    return page;
}

void vmm_map_page_to_frame(page_t* page, uint32_t frame_addr) {
	page->frame = frame_addr / PAGING_FRAME_SIZE;
}

page_t* vmm_page_alloc_for_phys_addr(page_directory_t* dir, uint32_t phys_addr) {
    page_t* page = vmm_get_page_for_virtual_address(dir, phys_addr);

	page->present = 1; //mark as present
    page->rw = true;
    page->user = false;

    pmm_alloc_address(phys_addr);
    vmm_map_page_to_frame(page, phys_addr);

    return page;
}

page_t* vmm_duplicate_frame_mapping(page_directory_t* dir, page_t* source, uint32_t dest_virt_addr) {
    page_t* dest = vmm_get_page_for_virtual_address(dir, dest_virt_addr);

	dest->present = source->present;
    dest->rw = source->rw;
    dest->user = source->user;
	dest->frame = source->frame;

    return dest;
}

page_t* vmm_page_alloc_for_virt_addr(page_directory_t* dir, uint32_t virt_addr) {
    page_t* page = vmm_get_page_for_virtual_address(dir, virt_addr);

	page->present = 1; //mark as present
	//page->rw = is_writeable; //should page be writable?
	//page->user = !is_kernel; //should page be user mode?
    page->rw = true;
    page->user = false;

    uint32_t frame_addr = pmm_alloc();
    vmm_map_page_to_frame(page, frame_addr);

    return page;
}

page_t* get_page(uint32_t address, int make, page_directory_t* dir) {
    NotImplemented();
    return NULL;
}

static void page_fault(register_state_t regs) {
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
	printf_err("page fault @ virt 0x%08x, flags: ", faulting_address);
	printf_err("%spresent", present ? "" : "not ");
	printf_err("%s operation", rw ? "write" : "read");
	printf_err("%s mode", us ? "user" : "kernel");

	if (reserved) printf_err("Overwrote CPU-resereved bits of page entry");
	if (id) printf_err("Faulted during instruction fetch");

	bool caused_by_execution = (regs.eip == faulting_address);
	printf_err("caused by %s unpaged memory", caused_by_execution ? "executing" : "reading");
    asm("sti");
    while (1) {}
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
