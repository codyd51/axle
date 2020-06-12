#include "vmm.h"
#include <kernel/pmm/pmm.h>
#include <std/kheap.h>
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <kernel/multitasking//tasks/task.h>
#include <kernel/boot_info.h>
#include <kernel/address_space.h>

#define PAGES_IN_PAGE_TABLE 1024
#define PAGE_TABLES_IN_PAGE_DIR 1024

static void page_fault(const register_state_t* regs);
static uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr);
static uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr);

# pragma mark - Control-register utility functions

static uint32_t _get_cr0() {
	uint32_t cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	return cr0;
}

static void _set_cr0(uint32_t cr0) {
	asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

uint32_t get_cr3() {
	uint32_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	return cr3;
}

static void _set_cr3(uint32_t addr) {
	asm volatile("mov %0, %%cr3" : : "r"(addr));
	int cr0 = _get_cr0();
	cr0 |= 0x80000000; //enable paging bit
	_set_cr0(cr0);
}

# pragma mark - VMM

volatile static vmm_page_directory_t* _loaded_pdir = 0;

bool vmm_is_active() {
    //check if paging bit is set in cr0
    uint32_t cr0 = _get_cr0();
    return cr0 & 0x80000000;
}

void vmm_load_pdir(vmm_page_directory_t* dir) {
    asm("cli");
    _set_cr3(dir);
    _loaded_pdir = dir;
    asm("sti");
}

vmm_page_directory_t* vmm_active_pdir() {
    return _loaded_pdir;
}

static uint32_t* _get_page_tables_head(vmm_page_directory_t* vmm_dir) {
    vmm_page_table_t* table_pointers = vmm_dir->table_pointers;
    uint32_t* ptr_raw = (uint32_t*)((uint32_t)table_pointers & PAGE_DIRECTORY_ENTRY_MASK);
    return ptr_raw;
}

static uint32_t* _get_raw_page_table_pointer_from_table_idx(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    return page_tables[page_table_idx];
}

static vmm_page_table_t* _get_page_table_from_table_idx(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    uint32_t* table_ptr_raw = _get_raw_page_table_pointer_from_table_idx(vmm_dir, page_table_idx);
    return (vmm_page_table_t*)((uint32_t)table_ptr_raw & PAGE_DIRECTORY_ENTRY_MASK);
}

static uint32_t _get_page_table_flags(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    uint32_t* table_ptr_raw = _get_raw_page_table_pointer_from_table_idx(vmm_dir, page_table_idx);
    return (uint32_t)table_ptr_raw & PAGE_TABLE_FLAG_BITS_MASK;
}

static bool vmm_page_table_is_present(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    return _get_page_table_flags(vmm_dir, page_table_idx) & PAGE_PRESENT_FLAG;
}

static void vmm_page_table_alloc(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    if (vmm_page_table_is_present(vmm_dir, page_table_idx)) {
        panic("table already allocd");
    }

    vmm_page_table_t* new_table = pmm_alloc();
    memset(new_table, 0, sizeof(vmm_page_table_t));
    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    page_tables[page_table_idx] = (uint32_t)new_table | PAGE_KERNEL_ONLY_FLAG | PAGE_READ_WRITE_FLAG | PAGE_PRESENT_FLAG;
    printf("alloced page table %d at frame 0x%x, new table = 0x%x\n", page_table_idx, new_table, _get_page_table_from_table_idx(vmm_dir, page_table_idx));
}

vmm_page_table_t* vmm_table_for_page_addr(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool alloc) {
    if (page_addr & ~PAGING_FRAME_MASK) {
        panic("vmm_table_for_page_addr is not page-aligned!");
    }
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(page_addr);

    if (!vmm_page_table_is_present(vmm_dir, table_idx)) {
        if (!alloc) {
            return NULL;
        }
        vmm_page_table_alloc(vmm_dir, table_idx);
    }

    vmm_page_table_t* page_table = _get_page_table_from_table_idx(vmm_dir, table_idx);
    return page_table;
}

void _vmm_set_page_table_entry(vmm_page_directory_t* vmm_dir, uint32_t page_addr, uint32_t frame_addr, bool present, bool readwrite, bool user_mode) {
    if (page_addr & PAGE_FLAG_BITS_MASK) {
        printf("page_addr 0x%x\n", page_addr);
        panic("page_addr is not page aligned");
    }
    if (frame_addr & PAGE_FLAG_BITS_MASK) {
        panic("frame_addr is not page aligned");
    }

    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_dir, page_addr, true);
    if (!table) {
        panic("failed to get page table");
    }
    if ((uint32_t)table & PAGE_TABLE_FLAG_BITS_MASK) {
        panic("table was not page-aligned");
    }

    //has this frame already been alloc'd?
    if (addr_space_bitmap_check_address(&vmm_dir->allocated_pages, page_addr)) {
        addr_space_bitmap_dump_set_ranges(&vmm_dir->allocated_pages);
        printf("page 0x%08x was alloc'd twice\n", page_addr);
        panic("VMM double alloc (bitmap)");
    }
    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);
    if (table->pages[page_idx].present) {
        panic("VMM double alloc (page table)");
    }

    table->pages[page_idx].frame_idx = frame_addr / PAGING_FRAME_SIZE;
    table->pages[page_idx].present = present;
    table->pages[page_idx].writable = readwrite;
    table->pages[page_idx].user_mode = user_mode;
    // Mark the page as allocated in the state bitmap
    addr_space_bitmap_set_address(&vmm_dir->allocated_pages, page_addr);
}

void vmm_alloc_page_address(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool readwrite) {
    uint32_t frame_addr = pmm_alloc();
    _vmm_set_page_table_entry(vmm_dir, page_addr, frame_addr, true, readwrite, false);
}

void vmm_alloc_page() {
    NotImplemented();
}
void vmm_identity_map_page(vmm_page_directory_t* vmm_dir, uint32_t frame_addr) {
    _vmm_set_page_table_entry(vmm_dir, frame_addr, frame_addr, true, true, false);
}


void vmm_map_region(vmm_page_directory_t* vmm_dir, uint32_t start_addr, uint32_t size) {
    NotImplemented();
}

void vmm_identity_map_region(vmm_page_directory_t* vmm_dir, uint32_t start_addr, uint32_t size) {
    if (start_addr & PAGE_FLAG_BITS_MASK) {
        panic("vmm_identity_map_region start not page aligned");
    }
    if (size & ~PAGING_FRAME_MASK) {
        panic("vmm_identity_map_region size not page aligned");
    }

    printf_dbg("Identity mapping from 0x%08x to 0x%08x", start_addr, start_addr + size);
    for (uint32_t addr = start_addr; addr < start_addr + size; addr += PAGE_SIZE) {
        vmm_identity_map_page(vmm_dir, addr);
    }
}

void vmm_free_page(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    NotImplemented();
}

static vmm_page_directory_t* _alloc_page_directory(void) {
    uint32_t* pd_head = pmm_alloc();
    uint32_t* pd_frame = pd_head;
    for (uint32_t i = 0; i < sizeof(vmm_page_directory_t); i += PAGING_FRAME_SIZE) {
        uint32_t* next_frame = pmm_alloc();
        assert((uint32_t)next_frame == (uint32_t)pd_frame + PAGING_FRAME_SIZE, "VMM dir alloc was not contiguous");
        pd_frame = next_frame;
    }
    memset(pd_head, 0, sizeof(vmm_page_directory_t));
    return (vmm_page_directory_t*)pd_head;
}

void vmm_init(void) {
    printf_info("Kernel VMM startup... ");

    vmm_page_directory_t* kernel_vmm_pd = _alloc_page_directory();

    boot_info_t* info = boot_info_get();
    info->vmm_kernel = kernel_vmm_pd;

    for (int i = 0; i < TABLES_IN_PAGE_DIRECTORY - 1; i++) {
        kernel_vmm_pd->table_pointers[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;
    }
    // Identity-map the lowest region of memory up to the end of the kernel image
    vmm_identity_map_region(kernel_vmm_pd, 0x0, info->kernel_image_end);

    // Identity-map a memory region just above the kernel image
    // This allows the PMM to allocate some frames before paging is enabled, 
    // and to have those frames accessible as-is once paging is enabled.
    // *** We currently reserve 1MB ***
    // *** This is defined both here and in pmm.c ***
    // *** It must be updated in both locations ***
    // *** TODO(PT): Define it elsewhere ***
    uint32_t extra_identity_mapped_block_size = 0x100000;
    vmm_identity_map_region(kernel_vmm_pd, info->kernel_image_end, extra_identity_mapped_block_size);

    interrupt_setup_callback(INT_VECTOR_INT14, (int_callback_t)page_fault);
    vmm_load_pdir(kernel_vmm_pd);

    vmm_alloc_page_address(kernel_vmm_pd, 0x500000, true);
    uint32_t* ptr = (uint32_t*)0x500000;
    *ptr = 0xdeadbeef;

    vmm_dump(kernel_vmm_pd);

    asm("cli");
    asm("hlt");
}

vmm_page_directory_t* vmm_clone_pdir(vmm_page_directory_t* source_vmm_dir) {
    vmm_page_directory_t* kernel_vmm_pd = boot_info_get()->vmm_kernel;
    vmm_page_directory_t* new_pd = pmm_alloc();
    printf("new_pd 0x%x\n", new_pd);
    for (uint32_t i = 0; i < PAGE_TABLES_IN_PAGE_DIR - 1; i++) {
        uint32_t* kernel_page_table = _get_raw_page_table_pointer_from_table_idx(kernel_vmm_pd, i);
        uint32_t kernel_page_table_flags = (uint32_t)kernel_page_table & PAGE_TABLE_FLAG_BITS_MASK;
        if (kernel_page_table_flags & PAGE_PRESENT_FLAG) {
            printf("table %d is present in kernel dir, will link\n", i);
            new_pd->table_pointers[i] = (uint32_t)kernel_page_table;
        }
        else {
            uint32_t* source_page_table = _get_raw_page_table_pointer_from_table_idx(source_vmm_dir, i);
            uint32_t source_page_table_flags = (uint32_t)source_page_table & PAGE_TABLE_FLAG_BITS_MASK;
            new_pd->table_pointers[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;
            NotImplemented();
        }
    }
}

uint32_t vmm_get_phys_for_virt(uint32_t virtualaddr) {
    NotImplemented();
}

static void page_fault(const register_state_t* regs) {
	//page fault has occured
	//faulting address is stored in CR2 register
	uint32_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	//error code tells us what happened
	int present = !(regs->err_code & 0x1); //page not present
	int rw = regs->err_code & 0x2; //write operation?
	int us = regs->err_code & 0x4; //were we in user mode?
	int reserved = regs->err_code & 0x8; //overwritten CPU-reserved bits of page entry?
	int id = regs->err_code & 0x10; //caused by instruction fetch?

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

	bool caused_by_execution = (regs->eip == faulting_address);
	printf("Caused by %s unpaged memory\n", caused_by_execution ? "executing" : "reading");
    printf("Kernel spinlooping due to unhandled page fault\n");
    //asm("sti");
    while (1) {}
}

void vmm_dump(vmm_page_directory_t* vmm_dir) {
	if (!vmm_dir) return;
    printf("Virtual memory manager state:\n");
    printf("\tLocated at physical address 0x%08x\n", vmm_dir);
    printf("\tMapped regions:\n");

	uint32_t run_start, run_end;
	bool in_run = false;
    uint32_t* page_directory = _get_page_tables_head(vmm_dir);
    uint32_t table_count_to_check = 1024;
	for (int i = 0; i < PAGE_TABLES_IN_PAGE_DIR; i++) {
        if (!vmm_page_table_is_present(vmm_dir, i)) {
            continue;
        }
        vmm_page_table_t* table = _get_page_table_from_table_idx(vmm_dir, i);
        printf("\tTable 0x%08x: 0x%08x - 0x%08x\n", table, i * PAGE_SIZE * PAGE_TABLES_IN_PAGE_DIR, (i+1) * PAGE_SIZE * PAGE_TABLES_IN_PAGE_DIR);

		//page tables map 1024 4kb pages
		//page directories contains 1024 page tables
		//therefore, each page table maps 4mb of the virtual addr space
		//for a given table index and page index, the virtual address is:
		//table index * (range mapped by each table) + page index * (range mapped by each page)
		//table index * 4mb + page index * 4kb
		int page_table_virt_range = PAGE_SIZE * PAGE_SIZE / 4;

		for (int j = 0; j < 1024; j++) {
			if (table->pages[j].present) {
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
    printf("\tBitmap:\n");
    addr_space_bitmap_dump_set_ranges(&vmm_dir->allocated_pages);
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
