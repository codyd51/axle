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
#include <kernel/util/spinlock/spinlock.h>

#define PAGES_IN_PAGE_TABLE 1024
#define PAGE_TABLES_IN_PAGE_DIR 1024

static bool _vmm_debug = false;
#define DBG_PAGING (_vmm_debug)
#define VAS_PRINTF(fmt, ...) if (DBG_PAGING) { printf(fmt, ##__VA_ARGS__); }

static void page_fault(const register_state_t* regs);
uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr);
uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr);
void vmm_unmap_range(vmm_page_directory_t* vmm_dir, uint32_t virt_start, uint32_t size);
uint32_t vmm_map_phys_range(vmm_page_directory_t* vmm_dir, uint32_t phys_start, uint32_t size);
uint32_t vas_active_map_phys_range(uint32_t phys_start, uint32_t size);
void vas_active_unmap_temp(uint32_t size);
uint32_t vas_active_map_temp(uint32_t phys_start, uint32_t size);
bool vmm_page_table_is_present(vmm_page_directory_t* vmm_dir, uint32_t page_table_idx);
vmm_page_table_t* _get_page_table_from_table_idx(vmm_page_directory_t* vmm_dir, uint32_t page_table_idx);
static vmm_page_directory_t* _alloc_page_directory(bool map_allocation_bitmap);
uint32_t _get_phys_page_table_pointer_from_table_idx(vmm_page_directory_t* vmm_dir, int page_table_idx);
vmm_page_table_t* vmm_table_for_page_addr(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool alloc);
uint32_t vmm_get_phys_address_for_mapped_page(vmm_page_directory_t* vmm_dir, uint32_t page_addr);

static volatile vmm_page_directory_t* _loaded_pdir = 0;
static bool _has_set_up_initial_page_directory = false;
static uint32_t _first_page_outside_shared_kernel_tables = 0;
static spinlock_t _vmm_global_spinlock = {0};

/*
 * Control-register utility functions
 */

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

/*
 * Address space bitmap allocator
 */

address_space_page_bitmap_t* _vmm_state_bitmap(vmm_page_directory_t* vmm_dir) {
    if (vmm_is_active() && vmm_dir == vmm_active_pdir()) {
        return (address_space_page_bitmap_t*)ACTIVE_PAGE_BITMAP_HEAD;
    }
    return &vmm_dir->allocated_pages;
}

static uint32_t find_free_region(vmm_page_directory_t* vmm_dir, uint32_t region_size, uint32_t start_address) {
    address_space_page_bitmap_t* bitmap = _vmm_state_bitmap(vmm_dir);

	uint32_t run_start_idx, run_end_idx;
	bool in_run = false;
    // Don't bother searching bit-frames that are below the provided address to start searching
    for (int i = BITMAP_WORD_INDEX(start_address); i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t vmm_pages_entry = bitmap->set[i];

        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            uint32_t page_addr = (i * PAGING_PAGE_SIZE * BITS_PER_BITMAP_ENTRY) + (j * PAGING_PAGE_SIZE);
            // Don't consider the word if it's before the provided start address
            if (page_addr < start_address) {
                continue;
            }

            if (vmm_pages_entry & (1 << j)) {
                if (in_run) {
                    // Run broken by inaccessible page
                    in_run = false;
                    int idx = BITMAP_BIT_INDEX(i, j);
                }
                continue;
            }

            if (!in_run) {
                in_run = true;
                run_start_idx = BITMAP_BIT_INDEX(i, j);
            }
            else {
                run_end_idx = BITMAP_BIT_INDEX(i, j);
                uint32_t run_size = run_end_idx - run_start_idx;
                if (run_size >= (region_size / PAGING_PAGE_SIZE)) {
                    return run_start_idx;
                }
            }
        }
    }
    panic("find_free_region() found nothing!");
    return 0;
}

static uint32_t first_usable_vmm_index(vmm_page_directory_t* vmm_dir) {
    address_space_page_bitmap_t* bitmap = _vmm_state_bitmap(vmm_dir);
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE - 32; i++) {
        uint32_t vmm_pages_entry = bitmap->set[i];
        //have all these pages already been allocated by VMM? (all bits set)
        if (!(~vmm_pages_entry)) {
            continue;
        }

        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            //is this page already allocated by VMM? (bit is on)
            if (vmm_pages_entry & (1 << j)) {
                continue;
            }
            return BITMAP_BIT_INDEX(i, j);
        }
    }
    panic("first_usable_vmm_index() found nothing!");
    return 0;
}

static address_space_page_bitmap_t* _vmm_bitmap_for_addr(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    if (page_addr < _first_page_outside_shared_kernel_tables) {
        //printf("Using shared bitmap for page 0x%08x\n", page_addr);
        return _vmm_state_bitmap(boot_info_get()->vmm_kernel);
    }

    return _vmm_state_bitmap(vmm_dir);
}

static void vmm_bitmap_set_addr(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    addr_space_bitmap_set_address(_vmm_bitmap_for_addr(vmm_dir, page_addr), page_addr);
}

static void vmm_bitmap_unset_addr(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    addr_space_bitmap_unset_address(_vmm_bitmap_for_addr(vmm_dir, page_addr), page_addr);
}

static bool vmm_bitmap_check_address(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    return addr_space_bitmap_check_address(_vmm_bitmap_for_addr(vmm_dir, page_addr), page_addr);
}

static void vmm_bitmap_dump_set_ranges(vmm_page_directory_t* vmm_dir) {
    addr_space_bitmap_dump_set_ranges(_vmm_state_bitmap(vmm_dir));
}


/*
 * VMM quering / loading
 */

bool vmm_is_active() {
    //check if paging bit is set in cr0
    uint32_t cr0 = _get_cr0();
    return cr0 & 0x80000000;
}

void vmm_load_pdir(vmm_page_directory_t* dir, bool pause_interrupts) {
    if (pause_interrupts) asm("cli");
    _set_cr3(dir);
    _loaded_pdir = dir;
    if (pause_interrupts) asm("sti");
}

uint32_t vmm_active_pdir() {
    return get_cr3();
}

/*
 * VMM debug
 */

void vmm_dump(vmm_page_directory_t* vmm_dir) {
	if (!vmm_dir) return;
    printf("Virtual memory manager state:\n");
    printf("\tLocated at 0x%08x\n", vmm_dir);
    printf("\tMapped regions:\n");
	uint32_t run_start, run_end;
	bool in_run = false;
    uint32_t page_directory = vmm_dir->table_pointers;
	for (uint32_t i = 0; i < PAGE_TABLES_IN_PAGE_DIR; i++) {
        if (!vmm_page_table_is_present(vmm_dir, i)) {
            continue;
        }
        vmm_page_table_t* table = _get_page_table_from_table_idx(vmm_dir, i);
        // printf("\t\tTable 0x%08x: 0x%08x - 0x%08x\n", table, i * PAGE_SIZE * PAGE_TABLES_IN_PAGE_DIR, (i+1) * PAGE_SIZE * PAGE_TABLES_IN_PAGE_DIR);

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
    printf("\tBitmap (ignore entries below 0x%08x):\n", _first_page_outside_shared_kernel_tables);
    vmm_bitmap_dump_set_ranges(vmm_dir);
}

/*
 * VMM validation
 */

void vmm_validate_shared_tables_in_sync(vmm_page_directory_t* vmm_modified, vmm_page_directory_t* vmm_clean) {
    address_space_page_bitmap_t* modified_allocator = _vmm_state_bitmap(vmm_modified);
    address_space_page_bitmap_t* clean_allocator = _vmm_state_bitmap(vmm_clean);

    for (uint32_t page_table_idx = 0; page_table_idx < TABLES_IN_PAGE_DIRECTORY; page_table_idx++) {
        if (!vmm_page_table_is_present(vmm_modified, page_table_idx) || !vmm_page_table_is_present(vmm_clean, page_table_idx)) {
            continue;
        }

        uint32_t clean_table_phys = _get_phys_page_table_pointer_from_table_idx(vmm_clean, page_table_idx) & PAGE_DIRECTORY_ENTRY_MASK;
        uint32_t modified_table_phys = _get_phys_page_table_pointer_from_table_idx(vmm_modified, page_table_idx) & PAGE_DIRECTORY_ENTRY_MASK;

        // Only look at page tables that are shared between the two directories
        if (modified_table_phys != clean_table_phys) {
            continue;
        }
		int page_table_virt_range = PAGE_SIZE * PAGE_SIZE / 4;
        int page_table_virt_base = page_table_virt_range * page_table_idx;

        for (int page_idx = 0; page_idx < 1024; page_idx++) {
            uint32_t page_addr = page_table_virt_base + (page_idx * PAGE_SIZE);

            if (vmm_bitmap_check_address(vmm_modified, page_addr) != vmm_bitmap_check_address(vmm_clean, page_addr)) {
                printf("Task %d changed allocation state of page 0x%08x\n", getpid(), page_addr);
                printf("Shared table %d @ phys 0x%08x\n", page_table_idx, clean_table_phys);
                printf("Shared page table maps 0x%08x - 0x%08x\n", page_table_virt_base, page_table_virt_base + page_table_virt_range);

                panic("Preempted task changed allocation state of shared page table");
            }
        }
    }
}

/*
 * VMM initialization
 */

void vmm_init(void) {
    printf_info("Kernel VMM startup... ");

    _vmm_global_spinlock.name = "[VMM global spinlock]";

    vmm_page_directory_t* kernel_vmm_pd = _alloc_page_directory(true);
    kernel_vmm_pd->allocated_pages.lock.name = "VMM kernel bmp lock";

    boot_info_t* info = boot_info_get();
    info->vmm_kernel = kernel_vmm_pd;

    // Identity map kernel code + data and other resources that we want to be identity-mapped after enabling paging
    // Some of these regions overlap with each other, which is fine. We just want to ensure every one of these 
    // regions is identity-mapped.

    // Identity map low memory, 
    vmm_identity_map_region(kernel_vmm_pd, 0x0, info->kernel_image_start);
    printf("kernel image start 0x%08x\n", info->kernel_image_start);
    // Each kernel ELF section,
    multiboot_elf_section_header_table_t symbol_table_info = info->symbol_table_info;
	elf_section_header_t* sh = (elf_section_header_t*)symbol_table_info.addr;
	uint32_t shstrtab = sh[symbol_table_info.shndx].addr;
	for (uint32_t i = 0; i < symbol_table_info.num; i++) {
		const char* name = (const char*)(shstrtab + sh[i].name);
        printf("VMM identity map ELF section %s\n", name);
        vmm_identity_map_region(kernel_vmm_pd, sh[i].addr, sh[i].size);
    }
    // Kernel symbol table and string table,
    vmm_identity_map_region(kernel_vmm_pd, info->kernel_elf_symbol_table.strtab, info->kernel_elf_symbol_table.strtabsz);
    vmm_identity_map_region(kernel_vmm_pd, info->kernel_elf_symbol_table.symtab, info->kernel_elf_symbol_table.symtabsz);
    // Kernel code+data,
    vmm_identity_map_region(kernel_vmm_pd, info->kernel_image_start, info->kernel_image_size);
    // Ramdisk,
    vmm_identity_map_region(kernel_vmm_pd, info->initrd_start, info->initrd_size);
    // And an extra region above the kernel image. 
    // This region allows the PMM to allocate some frames before paging is enabled, 
    // and to have those frames accessible as-is once paging is enabled.
    uint32_t extra_identity_mapped_block_size = 0x400000;
    vmm_identity_map_region(kernel_vmm_pd, info->kernel_image_end, extra_identity_mapped_block_size);
    // Identity map up to where the initial VMM was allocated
    uint32_t last_mapped_addr = info->kernel_image_end + extra_identity_mapped_block_size;
    uint32_t initial_page_dir_phys_end = (uint32_t)kernel_vmm_pd + sizeof(vmm_page_directory_t);
    vmm_identity_map_region(kernel_vmm_pd, last_mapped_addr, (initial_page_dir_phys_end - last_mapped_addr));

    // Allocate page tables up to 128MB
    uint32_t kernel_page_tables_max = 0x8000000;
    for (uint32_t addr = 0x0; addr < kernel_page_tables_max; addr += PAGE_SIZE * PAGES_IN_PAGE_TABLE) {
        vmm_page_table_t* table = vmm_table_for_page_addr(kernel_vmm_pd, addr, false);
        if (table) {
            printf("Kernel table already allocated: 0x%08x - 0x%08x\n", addr, addr + (PAGE_SIZE * PAGES_IN_PAGE_TABLE));
            continue;
        }
        table = vmm_table_for_page_addr(kernel_vmm_pd, addr, true);
        assert(table != NULL, "Failed to allocate page table");
        printf("Kernel allocated extra page table 0x%08x - 0x%08x\n", addr, addr + (PAGE_SIZE * PAGES_IN_PAGE_TABLE));
    }

    interrupt_setup_callback(INT_VECTOR_INT14, (int_callback_t)page_fault);
    vmm_load_pdir(kernel_vmm_pd, true);
    _has_set_up_initial_page_directory = true;

    vmm_dump(kernel_vmm_pd);
}

/*
 * Handling for allocations inside/outside globally shared kernel tables
 */

uint32_t _allocations_base_for_vmm(vmm_page_directory_t* vmm) {
    if (vmm == boot_info_get()->vmm_kernel) {
        return 0;
    }
    return _first_page_outside_shared_kernel_tables;
}

void vmm_notify_shared_kernel_memory_allocated() {
    // TODO(PT): Derive this
    _first_page_outside_shared_kernel_tables = 0x8000000;
}

static vmm_page_directory_t* _alloc_page_directory(bool map_allocation_bitmap) {
    uint32_t phys_page_dir_ptr = pmm_alloc_continuous_range(sizeof(vmm_page_directory_t));
    VAS_PRINTF("_alloc_page_directory made new dir [phys 0x%08x - 0x%08x]\n", phys_page_dir_ptr, phys_page_dir_ptr + sizeof(vmm_page_directory_t));
    vmm_page_directory_t* phys_page_dir = (vmm_page_directory_t*)phys_page_dir_ptr;
    vmm_page_directory_t* virt_page_dir = phys_page_dir;

    if (vmm_is_active()) {
        virt_page_dir = vas_active_map_phys_range(phys_page_dir_ptr, sizeof(vmm_page_directory_t));
    }

    memset((char*)virt_page_dir, 0, sizeof(vmm_page_directory_t));
    virt_page_dir->allocated_pages.lock.name = "VMM alloc lock";

    for (int i = 0; i < TABLES_IN_PAGE_DIRECTORY; i++) {
        virt_page_dir->table_pointers[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;
    }
    // Top page-table is used for a recursive PT mapping
    virt_page_dir->table_pointers[1023] = (uint32_t)phys_page_dir | PAGE_KERNEL_ONLY_FLAG | PAGE_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;

    // Extra page table is used to store the current allocation-state bitmap in a known location
    // Also used as a special temporary-allocation zone for working with remote VASses
    uint32_t phys_extra_page_table = pmm_alloc();
    VAS_PRINTF("Extra page table: 0x%08x\n", phys_extra_page_table);
    uint32_t* virt_extra_page_table = phys_extra_page_table;

    if (vmm_is_active()) {
        virt_extra_page_table = vas_active_map_phys_range(phys_extra_page_table, PAGING_FRAME_SIZE);
    }
    for (int i = 0; i < PAGES_IN_PAGE_TABLE; i++) {
        virt_extra_page_table[i] = 0x2;
    }
    virt_page_dir->table_pointers[1022] = phys_extra_page_table | PAGE_KERNEL_ONLY_FLAG | PAGE_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;
    VAS_PRINTF("New virt extra tab 0x%08x\n", virt_page_dir->table_pointers[1022]);
    if (vmm_is_active()) {
        vmm_unmap_range(vmm_active_pdir(), virt_extra_page_table, PAGING_FRAME_SIZE);
    }

    if (map_allocation_bitmap) {
        vmm_map_region_phys_to_virt(virt_page_dir, &phys_page_dir->allocated_pages, ACTIVE_PAGE_BITMAP_HEAD, sizeof(address_space_page_bitmap_t));
    }

    if (vmm_is_active()) {
        vmm_unmap_range(vmm_active_pdir(), virt_page_dir, sizeof(vmm_page_directory_t));
    }

    return phys_page_dir;
}

/*
 *  VMM access / manipulation
 */

uint32_t* _get_page_tables_head(vmm_page_directory_t* vmm_dir) {
    vmm_page_table_t* table_pointers;
    if (vmm_dir == vmm_active_pdir()) {
        table_pointers = (vmm_page_table_t*)ACTIVE_PAGE_DIRECTORY_HEAD;
    }
    else {
        // If we're setting up paging for the first time, assume we're working with physical pointers
        table_pointers = vmm_dir->table_pointers;
    }
    uint32_t* ptr_raw = (uint32_t*)((uint32_t)table_pointers & PAGE_DIRECTORY_ENTRY_MASK);
    return ptr_raw;
}

vmm_page_table_t* _get_page_table_from_table_idx(vmm_page_directory_t* vmm_dir, uint32_t page_table_idx) {
    vmm_page_table_t* table_pointers;
    if (vmm_dir == vmm_active_pdir()) {
        return 0xFFC00000 + (PAGE_SIZE * page_table_idx);
    }

    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    return (vmm_page_table_t*)(page_tables[page_table_idx] & PAGE_DIRECTORY_ENTRY_MASK);
    NotImplemented();
}

uint32_t _get_phys_page_table_pointer_from_table_idx(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    return page_tables[page_table_idx];
}

static uint32_t _get_page_table_flags(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    uint32_t* table_ptr_raw = _get_phys_page_table_pointer_from_table_idx(vmm_dir, page_table_idx);
    return (uint32_t)table_ptr_raw & PAGE_TABLE_FLAG_BITS_MASK;
}

bool vmm_page_table_is_present(vmm_page_directory_t* vmm_dir, uint32_t page_table_idx) {
    return _get_page_table_flags(vmm_dir, page_table_idx) & PAGE_PRESENT_FLAG;
}

uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr) {
    uint32_t page_idx = addr / PAGING_PAGE_SIZE;
    uint32_t table_idx = page_idx / PAGE_TABLES_IN_PAGE_DIR;
    return table_idx;
}

uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr) {

    uint32_t page_idx = addr / PAGING_PAGE_SIZE;
    return page_idx % PAGES_IN_PAGE_TABLE;
}

/*
 * Allocate pages & page tables
 */

static void vmm_page_table_alloc(vmm_page_directory_t* vmm_dir, int page_table_idx) {
    if (vmm_page_table_is_present(vmm_dir, page_table_idx)) {
        panic("table already allocd");
    }

    // Protect against allocating new tables in the shared kernel directory
    if (vmm_dir == boot_info_get()->vmm_kernel) {
        // Have we already allocated all the "static" kernel memory?
        if (_first_page_outside_shared_kernel_tables > 0) {
            printf("Cannot alloc new PTs in kernel dir after allocating static memory.\n");
            printf("Tried to alloc PT %d\n", page_table_idx);
            panic("Cannot alloc new page tables in kernel!");
        }
    }

    uint32_t new_table = pmm_alloc();
    VAS_PRINTF("Page table alloc @ 0x%08x (maps 0x%08x - 0x%08x)\n", new_table, (page_table_idx * 1024 * 1024 * 4), ((page_table_idx + 1) * 1024 * 1024 * 4));

    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    page_tables[page_table_idx] = (uint32_t)new_table | PAGE_KERNEL_ONLY_FLAG | PAGE_READ_WRITE_FLAG | PAGE_PRESENT_FLAG;
    invlpg(&page_tables[page_table_idx]);

    uint32_t* page_table_base = (uint32_t*)_get_page_table_from_table_idx(vmm_dir, page_table_idx);
    uint32_t* virt_page_table_base = page_table_base;
    uint32_t recursive_mapped_pt_addr = 0xFFC00000 + (PAGE_SIZE * page_table_idx);
    vmm_bitmap_set_addr(vmm_dir, recursive_mapped_pt_addr);

    for (int i = 0; i < PAGES_IN_PAGE_TABLE; i++) {
        virt_page_table_base[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG;
    }

    invlpg(page_table_base);
    invlpg(virt_page_table_base);
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

bool vas_virt_page_table_is_present(vmm_page_directory_t* vas_virt, uint32_t table_idx) {
    return (uint32_t)(vas_virt->table_pointers[table_idx]) & PAGE_PRESENT_FLAG;
}

void vas_virt_page_table_alloc(vmm_page_directory_t* vas_virt, uint32_t page_table_idx) {
    if (vas_virt_page_table_is_present(vas_virt, page_table_idx)) {
        panic("table already allocd");
    }
    uint32_t new_table = pmm_alloc();
    VAS_PRINTF("vas_virt_pt_alloc [P 0x%08x] idx %d\n", new_table, page_table_idx);

    //uint32_t* virt_mapped_new_table = vmm_map_phys_range(vmm_active_pdir(), new_table, PAGING_FRAME_SIZE);
    uint32_t* virt_mapped_new_table = vas_active_map_temp(new_table, PAGING_FRAME_SIZE);
    VAS_PRINTF("Mapped new page table [phys 0x%08x] [virt 0x%08x]\n", new_table, virt_mapped_new_table);

    for (int i = 0; i < PAGES_IN_PAGE_TABLE; i++) {
        virt_mapped_new_table[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG;
    }

    //vmm_unmap_range(vmm_active_pdir(), virt_mapped_new_table, PAGING_FRAME_SIZE);
    vas_active_unmap_temp(PAGING_FRAME_SIZE);

    uint32_t* page_tables = (uint32_t*)vas_virt->table_pointers;
    page_tables[page_table_idx] = (uint32_t)new_table | PAGE_KERNEL_ONLY_FLAG | PAGE_READ_WRITE_FLAG | PAGE_PRESENT_FLAG;
    invlpg(&page_tables[page_table_idx]);
    
    // Mark the table as in-use by its recursively mapped address
    uint32_t recursive_mapped_page_table = 0xffc00000 + (PAGE_SIZE * page_table_idx);
    vmm_bitmap_set_addr(vas_virt, recursive_mapped_page_table);
}

vmm_page_table_t* vas_virt_table_for_page_addr(vmm_page_directory_t* vas_virt, uint32_t page_addr, bool alloc) {
    if (page_addr & ~PAGING_FRAME_MASK) {
        panic("vmm_table_for_page_addr is not page-aligned!");
    }
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(page_addr);
    if (!vas_virt_page_table_is_present(vas_virt, table_idx)) {
        if (!alloc) {
            return NULL;
        }
        vas_virt_page_table_alloc(vas_virt, table_idx);
    }

    return (vmm_page_table_t*)((uint32_t)vas_virt->table_pointers[table_idx] & PAGE_DIRECTORY_ENTRY_MASK);
}

uint32_t vmm_alloc_global_kernel_memory(uint32_t size) {
    printf("vmm_alloc_global_kernel_memory 0x%08x\n", size);

    // Modifying structures shared across all processes
    spinlock_acquire(&_vmm_global_spinlock);

    vmm_page_directory_t* active_vmm = vmm_active_pdir();

    //if (active_vmm != boot_info_get()->vmm_kernel) panic("out of band global alloc");

    vmm_load_pdir(boot_info_get()->vmm_kernel, false);
	uint32_t start = vmm_alloc_continuous_range(vmm_active_pdir(), size, true, 0, false);
    uint32_t start_phys = vmm_get_phys_address_for_mapped_page(vmm_active_pdir(), start);
    printf("vmm_alloc_global_kernel_memory allocated 0x%08x - 0x%08x (phys start 0x%08x)\n", start, start + size, start_phys);

    vmm_load_pdir(active_vmm, false);

    if (_first_page_outside_shared_kernel_tables > 0 && start + size >= _first_page_outside_shared_kernel_tables) {
        panic("Allocating shared kernel memory crossed over into an unshared page table");
    }

    spinlock_release(&_vmm_global_spinlock);
    return start;
}

void vmm_free_global_kernel_memory(uint32_t addr, uint32_t size) {
    printf("vmm_free_global_kernel_memory 0x%08x - 0x%08x\n", addr, addr + size);

    if (addr + size >= _first_page_outside_shared_kernel_tables) {
        panic("Freed shared kernel memory in unshared page table");
    }

    // Modifying structures shared across all processes
    spinlock_acquire(&_vmm_global_spinlock);

    vmm_page_directory_t* active_vmm = vmm_active_pdir();

    vmm_load_pdir(boot_info_get()->vmm_kernel, false);

    for (uint32_t i = addr; i < addr + size; i += PAGE_SIZE) {
        uint32_t frame_addr = vmm_get_phys_address_for_mapped_page(vmm_active_pdir(), i);
        pmm_free(frame_addr);
        _vmm_unmap_page(vmm_active_pdir(), i);
    }

   // vmm_dump(vmm_active_pdir());

    vmm_load_pdir(active_vmm, false);
    //vmm_dump(vmm_active_pdir());

    spinlock_release(&_vmm_global_spinlock);
}

void _vmm_unmap_page(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    if (page_addr & PAGE_FLAG_BITS_MASK) {
        printf("page_addr 0x%x\n", page_addr);
        panic("page_addr is not page aligned");
    }
    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_dir, page_addr, true);
    if (!table) {
        panic("failed to get page table");
    }
    if ((uint32_t)table & PAGE_TABLE_FLAG_BITS_MASK) {
        panic("table was not page-aligned");
    }

    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);
    if (!table->pages[page_idx].present) {
        printf_err("double-unmap of 0x%08x", page_addr);
        //panic("VMM double-unmap");
        return;
    }
    memset(&table->pages[page_idx], 0, sizeof(vmm_page_t));
    invlpg(page_addr);
    // Mark the page as freed in the state bitmap
    vmm_bitmap_unset_addr(vmm_dir, page_addr);
}

void vmm_debug(bool on) {
    _vmm_debug = on;
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
    if (vmm_bitmap_check_address(vmm_dir, page_addr)) {
        //addr_space_bitmap_dump_set_ranges(_vmm_state_bitmap(vmm_dir));
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
    invlpg(page_addr);
    // Mark the page as allocated in the state bitmap
    vmm_bitmap_set_addr(vmm_dir, page_addr);
}

void _vmm_set_page_flags(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool readwrite, bool user_mode) {
    if (page_addr & PAGE_FLAG_BITS_MASK) {
        printf("page_addr 0x%x\n", page_addr);
        panic("page_addr is not page aligned");
    }

    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_dir, page_addr, true);

    if (!table) {
        panic("failed to get page table");
    }
    if ((uint32_t)table & PAGE_TABLE_FLAG_BITS_MASK) {
        panic("table was not page-aligned");
    }

    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);

    table->pages[page_idx].writable = readwrite;
    table->pages[page_idx].user_mode = user_mode;
    invlpg(page_addr);
}

uint32_t vmm_alloc_page_address(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool readwrite) {
    uint32_t frame_addr = pmm_alloc();
    _vmm_set_page_table_entry(vmm_dir, page_addr, frame_addr, true, readwrite, false);
    return frame_addr;
}

void vmm_set_page_usermode(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    // This function calls _get_page_tables_head and interacts with the "physical" 
    // address of a page table, and may be unsafe to call with anything other than
    // the active page directory
    //assert(vmm_dir == vmm_active_pdir(), "May only be called on the active PDir");

    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_dir, page_addr, true);
    if (!table) {
        panic("failed to get page table");
    }
    if ((uint32_t)table & PAGE_TABLE_FLAG_BITS_MASK) {
        panic("table was not page-aligned");
    }

    // The page must be allocated already
    if (!vmm_bitmap_check_address(vmm_dir, page_addr)) {
        panic("This function requires that the page be allocated already\n");
    }
    _vmm_set_page_flags(vmm_dir, page_addr, true, true);

    // The page table is now certainly allocated, and may be set to kernel-mode
    // Ensure the page table is set up for user-mode access
    // Enabling user-mode on a page requires the page table also have the user bit set
    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(page_addr);
    uint32_t phys_table = page_tables[table_idx];
    if (!(phys_table & PAGE_USER_MODE_FLAG)) {
        //printf("Enabling user-mode flag on page table [%d] (phys|flags 0x%08x)\n", table_idx, phys_table);
        page_tables[table_idx] = phys_table | PAGE_USER_MODE_FLAG;
        invlpg(&page_tables[table_idx]);
        invlpg(phys_table);
    }
}

uint32_t vmm_alloc_page_address_usermode(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool readwrite) {
    uint32_t frame_addr = pmm_alloc();

    // This will allocate the page table if necessary
    _vmm_set_page_table_entry(vmm_dir, page_addr, frame_addr, true, readwrite, false);
    vmm_set_page_usermode(vmm_dir, page_addr);

    return frame_addr;
}

/*
uint32_t vmm_alloc_page_address_usermode(vmm_page_directory_t* vmm_dir, uint32_t page_addr, bool readwrite) {
    // This function calls _get_page_tables_head and interacts with the "physical" 
    // address of a page table, and may be unsafe to call with anything other than
    // the active page directory
    assert(vmm_dir == vmm_active_pdir(), "May only be called on the active PDir");

    uint32_t frame_addr = pmm_alloc();
    // This will allocate the page table if necessary
    _vmm_set_page_table_entry(vmm_dir, page_addr, frame_addr, true, readwrite, true);

    // The page table is now certainly allocated, and may be set to kernel-mode
    // Ensure the page table is set up for user-mode access
    // Enabling user-mode on a page requires the page table also have the user bit set
    uint32_t* page_tables = _get_page_tables_head(vmm_dir);
    uint32_t table_idx = vmm_page_table_idx_for_virt_addr(page_addr);
    uint32_t phys_table = page_tables[table_idx];
    if (!(phys_table & PAGE_USER_MODE_FLAG)) {
        printf("Enabling user-mode flag on page table [%d] (phys|flags 0x%08x)\n", table_idx, phys_table);
        page_tables[table_idx] = phys_table | PAGE_USER_MODE_FLAG;
        invlpg(&page_tables[table_idx]);
        invlpg(phys_table);
    }
    return frame_addr;
}
*/

uint32_t vmm_alloc_page(vmm_page_directory_t* vmm_dir, bool readwrite) {
    uint32_t index = first_usable_vmm_index(vmm_dir);
    uint32_t page_address = index * PAGING_PAGE_SIZE;
    vmm_alloc_page_address(vmm_dir, page_address, readwrite);
    return page_address;
}

uint32_t vmm_alloc_page_for_frame(vmm_page_directory_t* vmm_dir, uint32_t frame_addr, bool readwrite) {
    VAS_PRINTF("vmm_alloc_page_for_frame 0x%08x\n", frame_addr);
    uint32_t index = first_usable_vmm_index(vmm_dir);
    uint32_t page_address = index * PAGING_PAGE_SIZE;
    VAS_PRINTF("page_addr 0x%08x\n", page_address);
    _vmm_set_page_table_entry(vmm_dir, page_address, frame_addr, true, readwrite, false);
    return page_address;
}

void _vas_virt_set_page_table_entry(vmm_page_directory_t* vas_virt, uint32_t page_addr, uint32_t frame_addr, bool present, bool readwrite, bool user_mode) {
    if (page_addr & PAGE_FLAG_BITS_MASK) {
        printf("page_addr 0x%x\n", page_addr);
        panic("page_addr is not page aligned");
    }
    if (frame_addr & PAGE_FLAG_BITS_MASK) {
        panic("frame_addr is not page aligned");
    }

    VAS_PRINTF("_vas_virt_set_pte [phys 0x%08x] [virt 0x%08x]\n", frame_addr, page_addr);
    uint32_t phys_table = vas_virt_table_for_page_addr(vas_virt, page_addr, true);
    //vmm_page_table_t* virt_table = vas_active_map_phys_range(phys_table, PAGING_FRAME_SIZE);
    vmm_page_table_t* virt_table = vas_active_map_temp(phys_table, PAGING_FRAME_SIZE);
    VAS_PRINTF("_vas_virt_set_pte got pt [P 0x%08x] [V 0x%08x]\n", phys_table, virt_table);

    if (!phys_table) {
        panic("failed to get page table");
    }
    if ((uint32_t)phys_table & PAGE_TABLE_FLAG_BITS_MASK) {
        panic("table was not page-aligned");
    }

    // has this frame already been alloc'd?
    if (vmm_bitmap_check_address(vas_virt, page_addr)) {
        vmm_bitmap_dump_set_ranges(vas_virt);
        printf("page 0x%08x was alloc'd twice\n", page_addr);
        panic("VMM double alloc (bitmap)");
    }

    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);
    if (virt_table->pages[page_idx].present) {
        panic("VMM double alloc (page table)");
    }

    virt_table->pages[page_idx].frame_idx = frame_addr / PAGING_FRAME_SIZE;
    virt_table->pages[page_idx].present = present;
    virt_table->pages[page_idx].writable = readwrite;
    virt_table->pages[page_idx].user_mode = user_mode;
    invlpg(page_addr);
    // Mark the page as allocated in the state bitmap
    vmm_bitmap_set_addr(vas_virt, page_addr);

    //vmm_unmap_range(vmm_active_pdir(), virt_table, PAGING_FRAME_SIZE);
    vas_active_unmap_temp(PAGING_FRAME_SIZE);
    /*
    uint32_t* virt_mapped_new_table = vmm_map_phys_range(vmm_active_pdir(), new_table, PAGING_FRAME_SIZE);
    printf("Mapped new page table [phys 0x%08x] [virt 0x%08x]\n", new_table, virt_mapped_new_table);

    for (int i = 0; i < PAGES_IN_PAGE_TABLE; i++) {
        virt_mapped_new_table[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG;
    }

    vmm_unmap_range(vmm_active_pdir(), virt_mapped_new_table, PAGING_FRAME_SIZE);
    */
   /*
    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);
    if (table->pages[page_idx].present) {
        panic("VMM double alloc (page table)");
    }

    table->pages[page_idx].frame_idx = frame_addr / PAGING_FRAME_SIZE;
    table->pages[page_idx].present = present;
    table->pages[page_idx].writable = readwrite;
    table->pages[page_idx].user_mode = user_mode;
    invlpg(page_addr);
    // Mark the page as allocated in the state bitmap
    addr_space_bitmap_set_address(_vmm_state_bitmap(vas_virt), page_addr);
    */
}

uint32_t vas_virt_alloc_page_address(vmm_page_directory_t* virt_vas, uint32_t page_address, bool readwrite) {
    VAS_PRINTF("vas_virt_alloc_page_address(0x%08x, 0x%08x)\n", virt_vas, page_address);
    uint32_t frame_addr = pmm_alloc();
    VAS_PRINTF("vas_virt_alloc_frame_address FRAME 0x%08x\n", frame_addr);
    _vas_virt_set_page_table_entry(virt_vas, page_address, frame_addr, true, readwrite, false);
    return frame_addr;
}

uint32_t vas_virt_alloc_continuous_range(vmm_page_directory_t* virt_vas, uint32_t size, bool readwrite) {
    if (size & PAGE_FLAG_BITS_MASK) {
        panic("size must be page-aligned");
    }
    uint32_t index = find_free_region(virt_vas, size, _allocations_base_for_vmm(virt_vas));
    uint32_t first_page_address = index * PAGING_PAGE_SIZE;
    VAS_PRINTF("vas_phys_alloc_continuous_range found region at 0x%08x\n", first_page_address);

    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = first_page_address + i;
        vas_virt_alloc_page_address(virt_vas, page_address, readwrite);
    }
    return first_page_address;
}

uint32_t vas_phys_alloc_continuous_range(uint32_t phys_vas, uint32_t size, bool readwrite) {
    if (size & PAGE_FLAG_BITS_MASK) {
        size = (size & PAGING_PAGE_MASK) + PAGING_PAGE_SIZE;
    }

    vmm_page_directory_t* virt_vas = (vmm_page_directory_t*)vmm_map_phys_range(vmm_active_pdir(), phys_vas, sizeof(vmm_page_directory_t));

    uint32_t first_page_address = vas_virt_alloc_continuous_range(virt_vas, size, readwrite);

    vmm_unmap_range(vmm_active_pdir(), virt_vas, sizeof(vmm_page_directory_t));

    return first_page_address;
}

uint32_t vas_active_map_phys_range(uint32_t phys_start, uint32_t size) {
    if (phys_start & PAGE_FLAG_BITS_MASK) {
        printf("phys_start 0x%x\n", phys_start);
        panic("phys_start is not page aligned");
    }
    if (size & PAGE_FLAG_BITS_MASK) {
        printf("size 0x%08x\n", size);
        panic("size is not page aligned");
    }

    // If we're mapping in the root directory, we're allowed to allocate within a shared page table
    // TODO(PT): Now that the shared tables bitmap allocation state is globally shared, 
    // we might not need to restrict allocations like this anymore.
    // However, we'll still need to guard against things like an allocation spilling into unshared memory
    // Otherwise, we must map within non-shared page tables
    uint32_t index = find_free_region(vmm_active_pdir(), size, _allocations_base_for_vmm(vmm_active_pdir()));
    uint32_t first_page_address = index * PAGING_PAGE_SIZE;

    VAS_PRINTF("vas_active_map_phys [P 0x%08x - 0x%08x] [V 0x%08x - 0x%08x]\n", phys_start, phys_start + size, first_page_address, first_page_address + size);
    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = first_page_address + i;
        _vmm_set_page_table_entry(vmm_active_pdir(), page_address, phys_start + i, true, true, false);
    }
    return first_page_address;
}

uint32_t vmm_set_continuous_range_to_phys(vmm_page_directory_t* vmm_dir, uint32_t phys_addr, uint32_t size, bool readwrite, uint32_t min_address) {
    if (size & PAGE_FLAG_BITS_MASK) {
        size = (size & PAGING_PAGE_MASK) + PAGING_PAGE_SIZE;
    }

    min_address = max(_allocations_base_for_vmm(vmm_dir), min_address);
    printf("vmm_set_continuous_range_to_phys MIN ADDRESS 0x%08x\n", min_address);
    uint32_t index = find_free_region(vmm_dir, size, min_address);
    uint32_t first_page_address = index * PAGING_PAGE_SIZE;
    printf("vmm_set_continuous_range_to_phys found region at 0x%08x\n", first_page_address);
    // Bug here if a user PDir allocates something in a shared table
    // The bitmap will be set as used in the user PDir but will not be marked as used in the kernel PDir
    // (Or any other PDir that shares the same table)
    // Maybe we keep a linked-list of VMMs that share a page table
    // Any time that page table is updated, we update all the users of the page table's allocation state bitmaps
    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = first_page_address + i;
        uint32_t frame_addr = phys_addr + i;
        _vmm_set_page_table_entry(vmm_dir, page_address, frame_addr, true, true, false);
    }
    return first_page_address;
}

uint32_t vmm_find_start_of_free_region(vmm_page_directory_t* vmm_dir, uint32_t size, uint32_t min_address) {
    min_address = max(_allocations_base_for_vmm(vmm_dir), min_address);
    uint32_t index = find_free_region(vmm_dir, size, min_address);
    uint32_t first_page_address = index * PAGING_PAGE_SIZE;
    return first_page_address;
}

uint32_t vmm_alloc_continuous_range(vmm_page_directory_t* vmm_dir, uint32_t size, bool readwrite, uint32_t min_address, bool usermode) {
    if (size & PAGE_FLAG_BITS_MASK) {
        size = (size & PAGING_PAGE_MASK) + PAGING_PAGE_SIZE;
    }

    min_address = max(_allocations_base_for_vmm(vmm_dir), min_address);
    uint32_t index = find_free_region(vmm_dir, size, min_address);
    uint32_t first_page_address = index * PAGING_PAGE_SIZE;
    // Bug here if a user PDir allocates something in a shared table
    // The bitmap will be set as used in the user PDir but will not be marked as used in the kernel PDir
    // (Or any other PDir that shares the same table)
    // Maybe we keep a linked-list of VMMs that share a page table
    // Any time that page table is updated, we update all the users of the page table's allocation state bitmaps
    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = first_page_address + i;
        if (usermode) {
            vmm_alloc_page_address_usermode(vmm_dir, page_address, readwrite);
        }
        else {
            vmm_alloc_page_address(vmm_dir, page_address, readwrite);
        }
    }
    return first_page_address;
}


void vmm_identity_map_page(vmm_page_directory_t* vmm_dir, uint32_t frame_addr) {
    _vmm_set_page_table_entry(vmm_dir, frame_addr, frame_addr, true, true, false);
}

void vmm_map_region(vmm_page_directory_t* vmm_dir, uint32_t start_addr, uint32_t size) {
    NotImplemented();
}

bool vmm_address_is_mapped(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    return vmm_bitmap_check_address(vmm_dir, page_addr);
}

uint32_t vmm_get_phys_address_for_mapped_page(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    if (!vmm_address_is_mapped(vmm_dir, page_addr)) {
        panic("Address is not mapped\n");
    }
    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_dir, page_addr, false);
    if (!table) {
        panic("failed to get page table");
    }
    uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);
    return table->pages[vmm_page_idx_within_table_for_virt_addr(page_addr)].frame_idx * PAGING_FRAME_SIZE;
}

void vmm_identity_map_region(vmm_page_directory_t* vmm_dir, uint32_t start_addr, uint32_t size) {
    start_addr = addr_space_page_floor(start_addr);
    size = addr_space_page_ceil(size);

    if (start_addr & PAGE_FLAG_BITS_MASK) {
        panic("vmm_identity_map_region start not page aligned");
    }
    if (size & ~PAGING_FRAME_MASK) {
        printf("size: 0x%08x\n", size);
        panic("vmm_identity_map_region size not page aligned");
    }

    printf("VMM Identity mapping from 0x%08x to 0x%08x\n", start_addr, start_addr + size);
    for (uint32_t addr = start_addr; addr < start_addr + size; addr += PAGE_SIZE) {
        if (vmm_address_is_mapped(vmm_dir, addr)) {
            uint32_t frame = vmm_get_phys_address_for_mapped_page(vmm_dir, addr);
            assert(frame == addr, "Expected page to be identity-mapped");
            continue;
        }
        vmm_identity_map_page(vmm_dir, addr);
    }
}

void vmm_map_region_phys_to_virt(vmm_page_directory_t* vmm_dir, uint32_t phys_start, uint32_t virt_start, uint32_t size) {
    if (phys_start & PAGE_FLAG_BITS_MASK) {
        panic("vmm_map_region_phys_to_virt phys_start start not page aligned");
    }
    if (virt_start & PAGE_FLAG_BITS_MASK) {
        panic("vmm_map_region_phys_to_virt virt_start start not page aligned");
    }
    if (size & ~PAGING_FRAME_MASK) {
        printf("size: 0x%08x\n", size);
        panic("vmm_map_region_phys_to_virt size not page aligned");
    }

    VAS_PRINTF("VAS 0x%08x: Mapping phys 0x%08x - 0x%08x to virt 0x%08x - 0x%08x\n", vmm_dir, phys_start, phys_start + size, virt_start, virt_start + size);
    for (uint32_t offset = 0; offset < size; offset += PAGING_FRAME_SIZE) {
        _vmm_set_page_table_entry(vmm_dir, virt_start + offset, phys_start + offset, true, true, false);
    }
}

void vmm_free_page(vmm_page_directory_t* vmm_dir, uint32_t page_addr) {
    NotImplemented();
}

uint32_t vmm_map_phys_range__min_placement_addr(
    vmm_page_directory_t* vmm_dir, 
    uint32_t phys_start, 
    uint32_t size, 
    uint32_t min_placement_addr,
    bool user_mode) {
    //Deprecated();
    printf_info("map phys region of %d kb", size / 1024);
    if (phys_start & PAGE_FLAG_BITS_MASK) {
        panic("physstart must be page-aligned");
    }
    if (size & PAGE_FLAG_BITS_MASK) {
        size = (size & PAGING_PAGE_MASK) + PAGING_PAGE_SIZE;
    }

    uint32_t index = find_free_region(vmm_dir, size, min_placement_addr);
    uint32_t first_page_address = index * PAGING_PAGE_SIZE;

    printf("Map contiguous physical range 0x%08x - 0x%08x\n", phys_start, phys_start + size);
    printf("                      to virt 0x%08x - 0x%08x\n", first_page_address, first_page_address + size);

    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = first_page_address + i;
        uint32_t frame_address = phys_start + i;
        _vmm_set_page_table_entry(vmm_dir, page_address, frame_address, true, true, false);
        if (user_mode) {
            vmm_set_page_usermode(vmm_dir, page_address);
        }
    }
    return first_page_address;
}

uint32_t vmm_map_phys_range(vmm_page_directory_t* vmm_dir, uint32_t phys_start, uint32_t size) {
    return vmm_map_phys_range__min_placement_addr(vmm_dir, phys_start, size, _allocations_base_for_vmm(vmm_dir), false);
}

uint32_t vmm_remote_map_phys_range(uint32_t phys_vmm_addr, uint32_t phys_start, uint32_t size, uint32_t min_address) {
    // Pad buffer size to page size
    size = (size + PAGE_SIZE) & PAGING_PAGE_MASK;

    // Map the remote VAS state into the active VAS
    vmm_page_directory_t* virt_remote_pdir = (vmm_page_directory_t*)vas_active_map_temp(phys_vmm_addr, sizeof(vmm_page_directory_t));

    min_address = max(_allocations_base_for_vmm(virt_remote_pdir), min_address);
    printf("remote MIN ADDRESS 0x%08x\n", min_address);

    uint32_t remote_start = find_free_region(virt_remote_pdir, size, min_address) * PAGING_PAGE_SIZE;
    for (int i = 0; i < size; i+=PAGE_SIZE) {
        uint32_t remote_addr = remote_start+i;
        uint32_t phys_addr = phys_start+i;
        _vas_virt_set_page_table_entry(virt_remote_pdir, remote_addr, phys_addr, true, true, false);
    }
    vas_active_unmap_temp(sizeof(vmm_page_directory_t));

    return remote_start;
}

void vmm_unmap_range(vmm_page_directory_t* vmm_dir, uint32_t virt_start, uint32_t size) {
    //printf("Unmapping 0x%08x - 0x%08x\n", virt_start, virt_start + size);
    if (virt_start & PAGE_FLAG_BITS_MASK) {
        panic("virtstart must be page-aligned");
    }
    if (size & PAGE_FLAG_BITS_MASK) {
        size = (size & PAGING_PAGE_MASK) + PAGING_PAGE_SIZE;
    }

    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = virt_start + i;
        _vmm_unmap_page(vmm_dir, page_address);
    }
}

/*
 * VMM temp-mapping region
 */

static int _last_tmp_alloc_end = 0xff800000;
static int _last_tmp_alloc_size = 0;
uint32_t vas_active_map_temp(uint32_t phys_start, uint32_t size) {
    VAS_PRINTF("vas_active_map_temp 0x%08x 0x%08x\n", _last_tmp_alloc_end, size);
    if (phys_start & PAGE_FLAG_BITS_MASK) {
        printf("phys_start 0x%x\n", phys_start);
        panic("phys_start is not page aligned");
    }
    if (size & PAGE_FLAG_BITS_MASK) {
        panic("size is not page aligned");
    }

    // Ensure the temp-map is available
    uint32_t temp_mapping_zone_addr = 0xff800000;
    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_active_pdir(), temp_mapping_zone_addr, false);
    if (!table) {
        panic("temp mapping zone is unavailable");
    }

    if (_last_tmp_alloc_end < temp_mapping_zone_addr) {
        panic("invalid state for tmp mapping");
    }

    uint32_t first_page_address = _last_tmp_alloc_end;
    VAS_PRINTF("vas_active_map_temp [P 0x%08x - 0x%08x] [V 0x%08x - 0x%08x]\n", phys_start, phys_start + size, first_page_address, first_page_address + size);
    for (uint32_t i = 0; i < size; i += PAGING_PAGE_SIZE) {
        uint32_t page_address = first_page_address + i;
        _vmm_set_page_table_entry(vmm_active_pdir(), page_address, phys_start + i, true, true, false);
    }
    _last_tmp_alloc_end = first_page_address + size;
    _last_tmp_alloc_size = size;
    return first_page_address;
}

void vas_active_unmap_temp(uint32_t size) {
    VAS_PRINTF("vas_active_unmap_temp 0x%08x 0x%08x\n", _last_tmp_alloc_end, size);
    uint32_t temp_mapping_zone_addr = 0xff800000;
    vmm_page_table_t* table = vmm_table_for_page_addr(vmm_active_pdir(), temp_mapping_zone_addr, false);
    if (!table) {
        panic("temp mapping zone is unavailable");
    }

    if (_last_tmp_alloc_end < temp_mapping_zone_addr) {
        panic("invalid state for tmp mapping");
    }
    if (_last_tmp_alloc_size < 0) {
        panic("invalid size for tmp mapping");
    }

    vmm_unmap_range(vmm_active_pdir(), _last_tmp_alloc_end-size, size);
    _last_tmp_alloc_end -= size;
    _last_tmp_alloc_size -= size;
}

/*
 * Remote VMM access and manipulation
 */

vmm_page_directory_t* vmm_clone_active_pdir() {
    vmm_page_directory_t* kernel_vmm_pd = boot_info_get()->vmm_kernel;
    vmm_page_directory_t* active_pd = vmm_active_pdir();

    vmm_page_directory_t* phys_new_pd = _alloc_page_directory(false);
    vmm_page_directory_t* virt_new_pd = (vmm_page_directory_t*)vas_active_map_temp(phys_new_pd, sizeof(vmm_page_directory_t));
    VAS_PRINTF("vmm_clone_active_pdir [P 0x%08x] [V 0x%08x]\n", phys_new_pd, virt_new_pd);
    uint32_t* new_page_tables = &virt_new_pd->table_pointers;

    // Copy the allocation state of the parent directory
    memcpy(_vmm_state_bitmap(virt_new_pd), _vmm_state_bitmap(active_pd), sizeof(address_space_page_bitmap_t));
    // But clear the allocation state of fixed-mappings that we'll allocate now
    for (uint32_t page_addr = 0xff800000; page_addr < 0xff800000 + (0x1000*1024); page_addr += PAGING_PAGE_SIZE) {
        vmm_bitmap_unset_addr(virt_new_pd, page_addr);
    }

    // Now that we've copied the parent address space's allocation bitmap, 
    // we can safely make changes to the allocation state.
    // Map the allocation bitmap to the fixed allocation-bitmap address
    VAS_PRINTF("\n");
    for (int i = 0; i < sizeof(address_space_page_bitmap_t); i += 0x1000) {
        uint32_t page_address = ACTIVE_PAGE_BITMAP_HEAD + i;
        uint32_t frame_address = (uint32_t)(&phys_new_pd->allocated_pages) + i;
        VAS_PRINTF("Mapping frame bitmap [P 0x%08x] [V 0x%08x]\n", frame_address, page_address);
        _vas_virt_set_page_table_entry(virt_new_pd, page_address, frame_address, true, true, false);
    }
    VAS_PRINTF("allocated bitmap\n");

    // Iterate all the pages except for the recursive mapping, and the the allocation-state/temp-zone mapping
    for (uint32_t i = 0; i < PAGE_TABLES_IN_PAGE_DIR - 2; i++) {
        uint32_t kernel_page_table_with_flags = kernel_vmm_pd->table_pointers[i];
        uint32_t* kernel_page_table_ptr = kernel_page_table_with_flags & PAGE_DIRECTORY_ENTRY_MASK;
        uint32_t kernel_page_table_flags = (uint32_t)kernel_page_table_with_flags & PAGE_TABLE_FLAG_BITS_MASK;

        // Link tables present in the kernel VAS
        if (kernel_page_table_flags & PAGE_PRESENT_FLAG) {
            //printf("linking kernel page table 0x%08x - 0x%08x\n", (i * PAGE_SIZE * PAGES_IN_PAGE_TABLE), ((i+1)*PAGE_SIZE*PAGES_IN_PAGE_TABLE));
            new_page_tables[i] = (uint32_t)kernel_page_table_ptr | PAGE_KERNEL_ONLY_FLAG | PAGE_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;
            continue;
        }

        // Copy pages present in the source VAS that's not also in the kernel VAS
        uint32_t* active_page_table_with_flags = ((uint32_t*)ACTIVE_PAGE_DIRECTORY_HEAD)[i];
        uint32_t active_page_table_flags = (uint32_t)active_page_table_with_flags & PAGE_TABLE_FLAG_BITS_MASK;

        if (!(active_page_table_flags & PAGE_PRESENT_FLAG)) {
            continue;
        }

        printf("table %d is NOT present in kernel dir, will copy (table 0x%08x)\n", i, kernel_page_table_with_flags);
        NotImplemented();

        /*
        uint32_t* source_page_table = _get_phys_page_table_pointer_from_table_idx(source_vmm_dir, i);
        uint32_t source_page_table_flags = (uint32_t)source_page_table & PAGE_TABLE_FLAG_BITS_MASK;
        new_pd->table_pointers[i] = PAGE_KERNEL_ONLY_FLAG | PAGE_NOT_PRESENT_FLAG | PAGE_READ_WRITE_FLAG;
        */
    }
    VAS_PRINTF("unmapping 0x%08x from dir 0x%08x\n", virt_new_pd, vmm_active_pdir());
    vas_active_unmap_temp(sizeof(vmm_page_directory_t));
    VAS_PRINTF("phys_new_pd 0x%08x\n", phys_new_pd);

    return phys_new_pd;
}

uint32_t vmm_get_phys_for_virt(uint32_t virtualaddr) {
    NotImplemented();
}

/*
 * Page fault handling
 */

static void page_fault(const register_state_t* regs) {
	//page fault has occured
	//faulting address is stored in CR2 register
	uint32_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	//error code tells us what happened
	int page_present = (regs->err_code & 0x1); //page not present
	int forbidden_write = regs->err_code & 0x2; //write operation?
	int faulted_in_user_mode = regs->err_code & 0x4; //were we in user mode?
	int overwrote_reserved_bits = regs->err_code & 0x8; //overwritten CPU-reserved bits of page entry?
	int invalid_ip = regs->err_code & 0x10; //caused by instruction fetch?

	//if execution reaches here, recovery failed or recovery wasn't possible
    printf("|----------------|\n");
    printf("|  Page Fault %d  |\n", getpid());
    printf("|-  0x%08x  -|\n", faulting_address);

	if (overwrote_reserved_bits) printf_err("Overwrote CPU-resereved bits of page entry");
	if (invalid_ip) printf_err("Faulted during instruction fetch");
    if (faulted_in_user_mode) {
        // There may be other failure modes...
        if (page_present) {
            printf("User-mode code tried to access a kernel address\n");
        }
        else {
            printf("User-mode code tried to access an unmapped address\n");
        }
    }

	bool caused_by_execution = (regs->eip == faulting_address);
    const char* reason = "run";
    if (!caused_by_execution) {
        reason = forbidden_write ? "write" : "read ";
    }
	printf("| Unmapped %s |\n", reason);
    printf("|- EIP = 0x%08x -|\n", regs->eip);
    printf("|- UserESP = 0x%08x -|\n", regs->useresp);
    printf("|----------------|\n");
    panic("page fault");
    while (1) {}
}
