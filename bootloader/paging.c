#include "paging.h"
#include "axle_boot_info.h"
#include <stdbool.h>

#define PAGE_DIRECTORIES_IN_PAGE_DIRECTORY_POINTER_TABLE (PAGE_SIZE / sizeof(pdpe_t))
#define PAGE_TABLES_IN_PAGE_DIRECTORY (PAGE_SIZE / sizeof(pde_t))
#define PAGES_IN_PAGE_TABLE (PAGE_SIZE / sizeof(pte_t))

// PML4E index starts at bit 39 and takes up 9 bits
#define VMA_PML4E_IDX(addr) ((addr >> 39) & 0x1ff)
// PDPE index starts at bit 30 and takes up 9 bits
#define VMA_PDPE_IDX(addr) ((addr >> 30) & 0x1ff)
// PDE index starts at bit 21 and takes up 9 bits
#define VMA_PDE_IDX(addr) ((addr >> 21) & 0x1ff)
// PTE index starts at bit 12 and takes up 9 bits
#define VMA_PTE_IDX(addr) ((addr >> 12) & 0x1ff)

#define VMEM_IN_PTE (PAGE_SIZE)
#define VMEM_IN_PDE (VMEM_IN_PTE * PAGES_IN_PAGE_TABLE)
#define VMEM_IN_PDPE (VMEM_IN_PDE * PAGE_TABLES_IN_PAGE_DIRECTORY)

typedef union pml4e {
	struct {
		uint64_t present:1;
		uint64_t writable:1;
		uint64_t user_mode:1;
		uint64_t write_through:1;
		uint64_t cache_disabled:1;
		uint64_t accessed:1;
		uint64_t ignored:1;
		uint64_t must_be_zero:2;
		uint64_t available:3;
		uint64_t page_dir_pointer_base:40;
		uint64_t available_high:11;
		uint64_t no_execute:1;
	} bits;
	efi_physical_address_t phys_addr;
} pml4e_t;

typedef union {
	struct {
		uint64_t present:1;
		uint64_t writable:1;
		uint64_t user_mode:1;
		uint64_t write_through:1;
		uint64_t cache_disabled:1;
		uint64_t accessed:1;
		uint64_t ignored:1;
		uint64_t must_be_zero:1;
		uint64_t ignored2:1;
		uint64_t available:3;
		uint64_t page_dir_base:40;
		uint64_t available_high:11;
		uint64_t no_execute:1;
	} bits;
	efi_physical_address_t phys_addr;
} pdpe_t;

typedef union {
	struct {
		uint64_t present:1;
		uint64_t writable:1;
		uint64_t user_mode:1;
		uint64_t write_through:1;
		uint64_t cache_disabled:1;
		uint64_t accessed:1;
		uint64_t dirty:1;
		uint64_t must_be_one:1;
		uint64_t global:1;
		uint64_t available:3;
		uint64_t pat:1;
		uint64_t reserved_must_be_zero:17;
		uint64_t page_base:22;
		uint64_t available_high:7;
		uint64_t contextual:4;
		uint64_t no_execute:1;
	} bits;
	efi_physical_address_t phys_addr;
} pdpe_1gb_t;

typedef union {
	struct {
		uint64_t present:1;
		uint64_t writable:1;
		uint64_t user_mode:1;
		uint64_t write_through:1;
		uint64_t cache_disabled:1;
		uint64_t accessed:1;
		uint64_t ignored:1;
		uint64_t must_be_zero:1;
		uint64_t ignored2:1;
		uint64_t available:3;
		uint64_t page_table_base:40;
		uint64_t available_high:11;
		uint64_t no_execute:1;
	} bits;
	efi_physical_address_t phys_addr;
} pde_t;

typedef union {
	struct {
		uint64_t present:1;
		uint64_t writable:1;
		uint64_t user_mode:1;
		uint64_t write_through:1;
		uint64_t cache_disabled:1;
		uint64_t accessed:1;
		uint64_t dirty:1;
		uint64_t use_page_attribute_table:1;
		uint64_t global_page:1;
		uint64_t available:3;
		uint64_t page_base:40;
		uint64_t available_high:7;
		// Available if PKE=0, otherwise memory protection key
		uint64_t contextual:4;
		uint64_t no_execute:1;

	} bits;
	efi_physical_address_t phys_addr;
} pte_t;

uint64_t map_region_1gb_pages(pml4e_t* page_mapping_level4, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start) {
	//printf("map_region_1gb_pages [phys 0x%p - 0x%p] to [virt 0x%p - 0x%p]\n", phys_start, phys_start + vmem_size - 1, vmem_start, vmem_start + vmem_size - 1);
	// TODO(PT): Should remaining size be padded to the nearest GB?
	uint64_t remaining_size = vmem_size;
	uint64_t current_frame = phys_start;
	uint64_t gigabyte = 1024LL * 1024LL * 1024LL;

	int page_directory_pointer_table_idx = VMA_PML4E_IDX(vmem_start);
	//printf("\tPDPE idx %ld\n", page_directory_pointer_table_idx);

	// TODO(PT): Refactor into get_or_create()?
	pdpe_1gb_t* page_directory_pointer_table = NULL;
	if (page_mapping_level4[page_directory_pointer_table_idx].bits.present) {
		// We've already created the necessary PDPT
		page_directory_pointer_table = (pdpe_1gb_t*)(page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base * PAGE_SIZE);
		//printf("\tRe-using existing PDPT 0x%p\n", page_directory_pointer_table);
	}
	else {
		efi_physical_address_t page_directory_pointer_table_addr = 0;
		efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, 1, &page_directory_pointer_table_addr);
		if (EFI_ERROR(status)) {
			printf("\tFailed to allocate page directory pointer table! %ld\n", status);
			return 0;
		}
		page_directory_pointer_table = (pdpe_1gb_t*)page_directory_pointer_table_addr;
		memset(page_directory_pointer_table, 0, PAGE_SIZE);
		//printf("\tAllocated new page directory pointer table at 0x%p\n", page_directory_pointer_table);

		page_mapping_level4[page_directory_pointer_table_idx].bits.present = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.writable = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.user_mode = false;
		page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base = page_directory_pointer_table_addr / PAGE_SIZE;
	}

	uint64_t page_directories_needed = (remaining_size + (gigabyte - 1)) / gigabyte;
	page_directories_needed = min(page_directories_needed, PAGE_DIRECTORIES_IN_PAGE_DIRECTORY_POINTER_TABLE);
	uint64_t first_page_directory = VMA_PDPE_IDX(vmem_start);
	//printf("\tvmem_size 0x%p page_dirs_needed %ld\n", vmem_size, page_directories_needed);

	for (int page_directory_iter_idx = 0; page_directory_iter_idx < page_directories_needed; page_directory_iter_idx++) {
		int page_directory_idx = page_directory_iter_idx + first_page_directory;

		if (page_directory_pointer_table[page_directory_idx].bits.present) {
			// We've already created the necessary page directory
			//gb_page = (pdpe_1gb_t*)(page_directory_pointer_table[page_directory_idx].bits.page_dir_base * PAGE_SIZE);
			//printf("\tRe-using existing huge page 0x%p\n", gb_page);
			printf("\tPDPE idx %ld already present!\n", page_directory_idx);
			return 0;
		}
		else {
			page_directory_pointer_table[page_directory_idx].bits.present = true;
			page_directory_pointer_table[page_directory_idx].bits.writable = true;
			page_directory_pointer_table[page_directory_idx].bits.user_mode = false;
			// TODO(PT): Should be provided by the caller
			page_directory_pointer_table[page_directory_idx].bits.global = true;
			// TODO(PT): Ensure current_frame lies on a GB boundary
			page_directory_pointer_table[page_directory_idx].bits.page_base = current_frame / gigabyte;
			page_directory_pointer_table[page_directory_idx].bits.must_be_one = 1;
			//printf("\tSet huge page #%ld = 0x%p\n", page_directory_idx, page_directory_pointer_table[page_directory_idx]);

			remaining_size -= gigabyte;
			current_frame += gigabyte;
		}
	}
}

uint64_t map_region_4k_pages(pml4e_t* page_mapping_level4, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start) {
	//printf("map_region [phys 0x%p - 0x%p] to [virt 0x%p - 0x%p]\n", phys_start, phys_start + vmem_size - 1, vmem_start, vmem_start + vmem_size - 1);
	uint64_t remaining_size = vmem_size;
	uint64_t current_frame = phys_start;
	uint64_t current_page = vmem_start;

	int page_directory_pointer_table_idx = VMA_PML4E_IDX(vmem_start);
	//printf("\tPDPE idx %ld\n", page_directory_pointer_table_idx);

	pdpe_t* page_directory_pointer_table = NULL;
	if (page_mapping_level4[page_directory_pointer_table_idx].bits.present) {
		// We've already created the necessary PDPT
		page_directory_pointer_table = (pdpe_t*)(page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base * PAGE_SIZE);
		//printf("\tRe-using existing PDPT 0x%p\n", page_directory_pointer_table);
	}
	else {
		efi_physical_address_t page_directory_pointer_table_addr = 0;
		efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, 1, &page_directory_pointer_table_addr);
		if (EFI_ERROR(status)) {
			printf("\tFailed to allocate page directory pointer table! %ld\n", status);
			return 0;
		}
		page_directory_pointer_table = (pdpe_t*)page_directory_pointer_table_addr;
		memset(page_directory_pointer_table, 0, PAGE_SIZE);
		//printf("\tAllocated new page directory pointer table at 0x%p\n", page_directory_pointer_table);

		page_mapping_level4[page_directory_pointer_table_idx].bits.present = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.writable = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.user_mode = false;
		page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base = page_directory_pointer_table_addr / PAGE_SIZE;
	}

	uint64_t page_directories_needed = (remaining_size + (VMEM_IN_PDPE - 1)) / VMEM_IN_PDPE;
	page_directories_needed = min(page_directories_needed, PAGE_DIRECTORIES_IN_PAGE_DIRECTORY_POINTER_TABLE);
	uint64_t first_page_directory = VMA_PDPE_IDX(vmem_start);
	//printf("\tvmem_size 0x%p page_dirs_needed %ld\n", vmem_size, page_directories_needed);

	for (int page_directory_iter_idx = 0; page_directory_iter_idx < page_directories_needed; page_directory_iter_idx++) {
	//for (int page_directory_idx = first_page_directory; page_directory_idx < first_page_directory + page_directories_needed; page_directory_idx++) {
		int page_directory_idx = page_directory_iter_idx + first_page_directory;
		//printf("\tpage_directory_idx %ld\n", page_directory_idx);
		pde_t* page_directory = NULL;
		if (page_directory_pointer_table[page_directory_idx].bits.present) {
			// We've already created the necessary page directory
			page_directory = (pde_t*)(page_directory_pointer_table[page_directory_idx].bits.page_dir_base * PAGE_SIZE);
			//printf("\tRe-using existing page directory 0x%p\n", page_directory);
		}
		else {
			efi_physical_address_t page_directory_addr = 0;
			efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, 1, &page_directory_addr);
			if (EFI_ERROR(status)) {
				printf("Failed to allocate page directory! %ld\n", status);
				return 0;
			}
			//printf("\tAllocated new page directory 0x%p\n", page_directory_addr);
			page_directory = (pde_t*)page_directory_addr;
			memset(page_directory, 0, PAGE_SIZE);

			page_directory_pointer_table[page_directory_idx].bits.present = true;
			page_directory_pointer_table[page_directory_idx].bits.writable = true;
			page_directory_pointer_table[page_directory_idx].bits.user_mode = false;
			page_directory_pointer_table[page_directory_idx].bits.page_dir_base = page_directory_addr / PAGE_SIZE;
		}

		uint64_t page_tables_needed = (remaining_size + (VMEM_IN_PDE - 1)) / VMEM_IN_PDE;
		page_tables_needed = min(page_tables_needed, PAGE_TABLES_IN_PAGE_DIRECTORY);
		uint64_t first_page_table = VMA_PDE_IDX(current_page);

		for (int page_table_iter_idx = 0; page_table_iter_idx < page_tables_needed; page_table_iter_idx++) {
			int page_table_idx = page_table_iter_idx + first_page_table;
			//printf("\tpage_table_idx %ld\n", page_table_idx);
			pte_t* page_table = NULL;
			if (page_directory[page_table_idx].bits.present) {
				// We've already created the necessary page table
				page_table = (pte_t*)(page_directory[page_table_idx].bits.page_table_base * PAGE_SIZE);
				//printf("\tRe-using existing page table 0x%p in page directory 0x%p\n", page_table, page_directory);
			}
			else {
				efi_physical_address_t page_table_addr = 0;
				efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, 1, &page_table_addr);
				if (EFI_ERROR(status)) {
					printf("Failed to allocate page table! %ld\n", status);
					return 0;
				}
				//printf("\tAllocated new page table 0x%p in page directory 0x%p\n", page_table_addr, page_directory);
				page_table = (pte_t*)page_table_addr;
				memset(page_table, 0, PAGE_SIZE);

				page_directory[page_table_idx].bits.present = true;
				page_directory[page_table_idx].bits.writable = true;
				page_directory[page_table_idx].bits.user_mode = false;
				page_directory[page_table_idx].bits.page_table_base = page_table_addr / PAGE_SIZE;
			}

			uint64_t pages_needed = (remaining_size + (VMEM_IN_PTE - 1)) / VMEM_IN_PTE;
			uint64_t first_page = VMA_PTE_IDX(current_page);
			pages_needed = min(pages_needed, PAGES_IN_PAGE_TABLE - first_page);
			//printf("pages needed: %ld, first_page %ld\n", pages_needed, first_page);

			for (int page_iter_idx = 0; page_iter_idx < pages_needed; page_iter_idx++) {
				int page_idx = page_iter_idx + first_page;
				//printf("\tpage idx %ld\n", page_idx);

				page_table[page_idx].bits.present = true;
				page_table[page_idx].bits.writable = true;
				page_table[page_idx].bits.user_mode = false;
				page_table[page_idx].bits.page_base = current_frame / PAGE_SIZE;
				//page_table[j].bits.global_page = true;

				remaining_size -= PAGE_SIZE;
				current_frame += PAGE_SIZE;
				current_page += PAGE_SIZE;
			}
		}
	}
}

/* 
Virtual Memory map

First 4G of memory is identity-mapped
First 2G of memory is mapped to 0xFFFFFFFF80000000
Kernel is mapped from UEFI-allocated memory to 0xFFFFFFFF80000000
Initrd is mapped from UEFI-allocated memory to 0xFFFFFFFF60000000

Remove 
*/

pml4e_t* map2(void) {
	efi_physical_address_t page_mapping_level4_addr = 0;
	efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, 1, &page_mapping_level4_addr);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate PML4! %ld\n", status);
		return 0;
	}
	pml4e_t* page_mapping_level4 = (pml4e_t*)page_mapping_level4_addr;
	memset(page_mapping_level4, 0, PAGE_SIZE);

	// PT: Each constant must individually be suffixed by LL, or else the 
	// product will be truncated to a uint32_t
	// Also, use %p instead of %16x
	//map_region(page_mapping_level4, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0);

	// Map physical RAM 
	// TODO(PT): Move into the kernel?
	//map_region_1gb_pages(page_mapping_level4, 0xFFFF800000000000, (1024LL * 1024LL * 1024LL * 64LL), 0x0);
	uint64_t max_ram_in_gb = 64LL;
	map_region_1gb_pages(page_mapping_level4, 0x0, (1024LL * 1024LL * 1024LL * max_ram_in_gb), 0x0);
	map_region_1gb_pages(page_mapping_level4, 0xFFFF800000000000LL, (1024LL * 1024LL * 1024LL * max_ram_in_gb), 0x0);

	// No need to trash the low identity map on kernel entry
	// Instead let the kernel init thread exit after spawning some new threads
	// The new threads will only have the high PML4 entries copied in, 
	// and the low mapping will be trashed once the process is killed
	// The process teardown will only need to free the PML4E as the PDPEs won't
	// have any pointers - they'll all be free pages.

	//printf("Allocated PML4 at 0x%p\n", page_mapping_level4);
	return page_mapping_level4;
}
