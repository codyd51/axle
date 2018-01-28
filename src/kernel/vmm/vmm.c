#include "vmm.h"
#include <kernel/pmm/pmm.h>
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/boot_info.h>
#include <kernel/address_space.h>

#define PAGES_IN_PAGE_TABLE 1024
#define PAGE_TABLES_IN_PAGE_DIR 1024

static void page_fault(register_state_t regs);
static uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr);
static uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr);
static void vmm_page_table_alloc_for_virt_addr(page_directory_t* dir, uint32_t addr);
static void vmm_remap_kernel(page_directory_t* dir, uint32_t dest_virt_addr);
static bool vmm_is_active();

void * get_physaddr(void * virtualaddr);

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

void set_cr3(uint32_t addr) {
	asm volatile("mov %0, %%cr3" : : "r"(addr));
	int cr0 = get_cr0();
	cr0 |= 0x80000000; //enable paging bit
	set_cr0(cr0);
}

//since the current page_directory structure isn't page-aligned, it's easier to statically allocate it than pmm-alloc it.
static page_directory_t kernel_directory __attribute__((aligned(PAGING_FRAME_SIZE))) = {0};
static vmm_pdir_t* _loaded_pdir = 0;

static bool vmm_is_active() {
    //check if paging bit is set in cr0
    uint32_t cr0 = get_cr0();
    return cr0 & 0x80000000;
}

void vmm_load_pdir(vmm_pdir_t* dir) {
    asm("cli");
    set_cr3(dir->physicalAddr);
    _loaded_pdir = dir;
    asm("sti");
}

vmm_pdir_t* vmm_active_pdir() {
    return _loaded_pdir;
}

void vmm_init(void) {
    printf_info("Kernel VMM startup...");

    boot_info_t* info = boot_info_get();

    info->vmm_kernel = &kernel_directory;
    //we know tablesPhysical is the physical address because paging isn't enabled yet
	kernel_directory.physicalAddr = (uint32_t)&kernel_directory.tablesPhysical;

    //identity-map everything up to the kernel image end, plus a little extra space
    //the extra space is to allow the PMM to allocate a few frames before paging is enabled
    //we reserve 1mb
    //NOTE: this variable is defined both here and in pmm.c
    //if you update it here, you must update it there are well, and vice versa
    //TODO(PT): make this more rigorous
    uint32_t extra_identity_map_region_size = 0x100000;
    printf("identity map from [0x%08x to 0x%08x]\n", 0x0, info->kernel_image_end + extra_identity_map_region_size);
    vmm_identity_map_region(&kernel_directory, 0x0, info->kernel_image_end + extra_identity_map_region_size);

    //map last PDE to the page directory itself
    //this is a trick to read/write to the page directory after it's been loaded
    //TODO(PT): add links here, tired now
    kernel_directory.tablesPhysical[1023] = kernel_directory.physicalAddr | 0x7;

    //vmm_dump(&kernel_directory);

	//before we enable paging, register page fault handler
	interrupt_setup_callback(INT_VECTOR_INT14, page_fault);

	//enable paging
	vmm_load_pdir(&kernel_directory);
}

void * get_physaddr(void * virtualaddr) {
    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long * pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    if (!(pd[pdindex])) {
        panic("no pd entry");
    }

    unsigned long * pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    if (!(pt[ptindex])) {
        panic("no pt entry");
    }

    return (void *)((pt[ptindex] & ~0xFFF) + ((unsigned long)virtualaddr & 0xFFF));
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
    printf("VMM Page Fault\n---------------\n");
    printf("Faulted on accessing 0x%08x\n", faulting_address);
    printf("Access flags:\n");
	printf("\t%spresent\n", present ? "" : "not ");
	printf("\t%s operation\n", rw ? "write" : "read");
	printf("\t%s mode\n", us ? "user" : "kernel");

	if (reserved) printf_err("Overwrote CPU-resereved bits of page entry");
	if (id) printf_err("Faulted during instruction fetch");

	bool caused_by_execution = (regs.eip == faulting_address);
	printf("Caused by %s unpaged memory\n", caused_by_execution ? "executing" : "reading");
    printf("Kernel spinlooping due to unhandled page fault\n");
    asm("sti");
    while (1) {}
}

page_t* vmm_get_page_for_virtual_address(page_directory_t* dir, uint32_t virt_addr) {
    if (virt_addr & ~PAGING_FRAME_MASK) {
        panic("vmm_page_for_virtual_address() frame address not page-aligned");
    }
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(virt_addr);
    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(virt_addr);

    //does this page table already exist?
    if (!dir->tables[table_idx]) {
        vmm_page_table_alloc_for_virt_addr(dir, virt_addr);
    }
    //printf("page table 0x%08x\n", dir->tables[table_idx]);
    //page_t* page = &(dir->tablesPhysical[table_idx]->pages[page_in_table_idx]);
    page_t* page = &(dir->tables[table_idx]->pages[page_idx]);

    return page;
}

void vmm_map_page_to_frame(page_t* page, uint32_t frame_addr) {
	page->frame = frame_addr / PAGING_FRAME_SIZE;
}

static void _active_vmm_map_virt_to_phys(vmm_pdir_t* dir, uint32_t page_addr, uint32_t frame_addr, uint16_t flags) {
    // Make sure that both addresses are page-aligned.
    unsigned long pdindex = (unsigned long)page_addr >> 22;
    unsigned long ptindex = (unsigned long)page_addr >> 12 & 0x03FF;

    unsigned long * pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // When it is not present, you need to create a new empty PT and
    // adjust the PDE accordingly.
    if (!(pd[pdindex])) {
        pd[pdindex] = pmm_alloc() | 0x07; //present, rw, us
    }

    unsigned long * pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    // When it is, then there is already a mapping present. What do you do now?
    if (pt[ptindex]) {
        panic("tried to overwrite existing mapping?");
    }

    pt[ptindex] = ((unsigned long)frame_addr) | (flags & 0xFFF) | 0x01; // Present

    // Now you need to flush the entry in the TLB
    // or you might not notice the change.
    uint32_t cr3 = get_cr3();
	asm volatile("mov %0, %%cr3" : : "r"(cr3));
}

void vmm_map_virt_to_phys(vmm_pdir_t* dir, uint32_t page_addr, uint32_t frame_addr) {
    if (vmm_is_active()) {
        if (vmm_active_pdir() == dir) {
            _active_vmm_map_virt_to_phys(dir, page_addr, frame_addr, 0x00);
            return;
        }
    }
    page_t* page = vmm_get_page_for_virtual_address(dir, frame_addr);

	page->present = 1; //mark as present
    page->rw = true;
    page->user = false;

    pmm_alloc_address(frame_addr);
    vmm_map_page_to_frame(page, frame_addr);
}

void vmm_map_virt(page_directory_t* dir, uint32_t page_addr) {
    vmm_map_virt_to_phys(dir, page_addr, pmm_alloc());
}

page_t* vmm_duplicate_frame_mapping(page_directory_t* dir, page_t* source, uint32_t dest_virt_addr) {
    page_t* dest = vmm_get_page_for_virtual_address(dir, dest_virt_addr);

	dest->present = source->present;
    dest->rw = source->rw;
    dest->user = source->user;
	dest->frame = source->frame;

    return dest;
}

void vmm_dump(page_directory_t* dir) {
    Deprecated();
	if (!dir) return;
    printf("Virtual memory manager state:\n");
    printf("\tLocated at physical address 0x%08x\n", dir);
    printf("\tMapped regions:\n");

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
					printf("\t[0x%08x - 0x%08x]\n", run_start, run_end);

					in_run = false;
				}
			}
		}
	}
}

static uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr) {
    uint32_t page_idx = addr / PAGING_PAGE_SIZE;
    uint32_t table_idx = page_idx / PAGE_TABLES_IN_PAGE_DIR;
    return table_idx;
}

static uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr) {
    uint32_t page_idx = addr / PAGING_PAGE_SIZE;
    return page_idx % PAGES_IN_PAGE_TABLE;
}

static void vmm_page_table_alloc_for_virt_addr(page_directory_t* dir, uint32_t virt_addr) {
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(virt_addr);
    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(virt_addr);
    //does this page table already exist?
    if (dir->tables[table_idx]) {
        panic("called vmm_page_table_alloc_for_virt_addr() on existing page table");
        return;
    }
    if (virt_addr >> 22 != table_idx) {
        panic("table_idx did not match");
    }

    //this relies on a page table being a frame size, so verify this assumption
    assert(PAGING_FRAME_SIZE == sizeof(page_table_t), "page_table_t was a different size from a frame");
    //TODO (PT) add a check here for whether paging is active here
    //if it is, maybe it should be a virtual memory allocator?
    //high when i wrote this so feel free to discard if incorrect/wrong assumption`
    //1/23 high again, the above seems to be correct.
    //maybe a _vmm_internal_alloc() that either does pmm or vmm allocation,
    //based on whether paging is enabled.
	//PRESENT, RW, US
    uint32_t table_page_flags = 0x7;
    if (!vmm_is_active()) {
        uint32_t identity_mapped_table_addr = pmm_alloc();
        printf("page table alloc 0x%08x\n", identity_mapped_table_addr);
        page_table_t* identity_mapped_table = (page_table_t*)identity_mapped_table_addr;

    	dir->tablesPhysical[table_idx] = (identity_mapped_table_addr) | table_page_flags;
    	dir->tables[page_idx] = identity_mapped_table;
    	memset(dir->tables[page_idx], 0, sizeof(page_table_t));
    }
    else {
        /*
        uint32_t frame_addr = pmm_alloc();
        printf("vmm-active page table alloc, virt 0x%08x phys 0x%08x\n", virt_addr, frame_addr);

        dir->tables[table_idx] = frame_addr;
        dir->tablesPhysical[table_idx] = (frame_addr) | table_page_flags;
        uint32_t cr3 = get_cr3();
        printf("reloading cr3 (current physicalAddr 0x%08x)\n", cr3);
        set_cr3(dir);
        //asm("movl %cr3, %eax");
        //asm("movl %eax,%cr3");
        printf("memesetting table\n");
        memset(dir->tables[table_idx], 0, sizeof(page_table_t));
        printf("done");
        */
        panic("vmm-active page table alloc");

        /*
        page_table_t (*vmm_virtual_page_tables)[1024] = (page_table_t(*)[1024])0xFFC00000;
        uint32_t (*virtual_page_dir)[1024] = (uint32_t(*)[1024])0xFFFFF000;
        */
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

void vmm_map_region(page_directory_t* dir, uint32_t start, uint32_t size) {
    if (start & ~PAGING_FRAME_MASK) {
        panic("vmm_map_region start not page aligned!");
    }
    if (size & ~PAGING_FRAME_MASK) {
        panic("vmm_map_region size not page aligned!");
    }

    printf_dbg("VMM mapping from 0x%08x to 0x%08x", start, start + size);
    int frame_count = size / PAGING_PAGE_SIZE;
    for (int i = 0; i < frame_count; i++) {
        uint32_t page_addr = start + (i * PAGING_PAGE_SIZE);
        printf_dbg("VMM allocing virt page 0x%08x", page_addr);
        vmm_map_virt(dir, page_addr);
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
        uint32_t frame_addr = start + (i * PAGING_PAGE_SIZE);
        uint32_t page_addr = frame_addr;
        vmm_map_virt_to_phys(dir, page_addr, frame_addr);
    }
}
