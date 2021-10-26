#include <uefi.h>
#include <stdbool.h>

#include "elf.h"
#include "axle_boot_info.h"

#define PAGE_SIZE 0x1000
#define PAGE_MASK ~(0xFFF)

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

// Define _fltused, since we're not linking against the MS C runtime, but use
// floats.
// https://fantashit.com/undefined-symbol-fltused-when-compiling-to-x86-64-unknown-uefi/
int _fltused = 0;

typedef union {
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

void hexdump(const void* addr, const int len) {
    const char* desc = NULL;
    int i;
    unsigned char buff[65];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    else if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 64) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf ("%02x ", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 64] = '.';
        else
            buff[i % 64] = pc[i];
        buff[(i % 64) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 64) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}

uint64_t map_region_1gb_pages(pml4e_t* page_mapping_level4, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start) {
	printf("map_region_1gb_pages %p %p %p\n", vmem_start, vmem_size, phys_start);
	printf("map_region_1gb_pages [phys 0x%p - 0x%p] to [virt 0x%p - 0x%p]\n", phys_start, phys_start + vmem_size - 1, vmem_start, vmem_start + vmem_size - 1);
	// TODO(PT): Should remaining size be padded to the nearest GB?
	uint64_t remaining_size = vmem_size;
	uint64_t current_frame = phys_start;
	uint64_t gigabyte = 1024LL * 1024LL * 1024LL;

	int page_directory_pointer_table_idx = VMA_PML4E_IDX(vmem_start);
	printf("\tPDPE idx %ld\n", page_directory_pointer_table_idx);

	// TODO(PT): Refactor into get_or_create()?
	pdpe_1gb_t* page_directory_pointer_table = NULL;
	if (page_mapping_level4[page_directory_pointer_table_idx].bits.present) {
		// We've already created the necessary PDPT
		page_directory_pointer_table = (pdpe_1gb_t*)(page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base * PAGE_SIZE);
		printf("\tRe-using existing PDPT 0x%p\n", page_directory_pointer_table);
	}
	else {
		efi_physical_address_t page_directory_pointer_table_addr = 0;
		efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_directory_pointer_table_addr);
		if (EFI_ERROR(status)) {
			printf("\tFailed to allocate page directory pointer table! %ld\n", status);
			return 0;
		}
		page_directory_pointer_table = (pdpe_1gb_t*)page_directory_pointer_table_addr;
		memset(page_directory_pointer_table, 0, PAGE_SIZE);
		printf("\tAllocated new page directory pointer table at 0x%p\n", page_directory_pointer_table);

		page_mapping_level4[page_directory_pointer_table_idx].bits.present = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.writable = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.user_mode = false;
		page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base = page_directory_pointer_table_addr / PAGE_SIZE;
	}

	uint64_t page_directories_needed = (remaining_size + (gigabyte - 1)) / gigabyte;
	page_directories_needed = min(page_directories_needed, PAGE_DIRECTORIES_IN_PAGE_DIRECTORY_POINTER_TABLE);
	uint64_t first_page_directory = VMA_PDPE_IDX(vmem_start);
	printf("\tvmem_size 0x%p page_dirs_needed %ld\n", vmem_size, page_directories_needed);

	for (int page_directory_iter_idx = 0; page_directory_iter_idx < page_directories_needed; page_directory_iter_idx++) {
		int page_directory_idx = page_directory_iter_idx + first_page_directory;
		printf("\tpage_directory_idx %ld\n", page_directory_idx);

		if (page_directory_pointer_table[page_directory_idx].bits.present) {
			// We've already created the necessary page directory
			//gb_page = (pdpe_1gb_t*)(page_directory_pointer_table[page_directory_idx].bits.page_dir_base * PAGE_SIZE);
			//printf("\tRe-using existing huge page 0x%p\n", gb_page);
			printf("\tPDPE idx %ld already present!\n", page_directory_idx);
			return 0;
		}
		else {
			/*j
			efi_physical_address_t gb_page_addr = 0;
			efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &gb_page_addr);
			if (EFI_ERROR(status)) {
				printf("Failed to allocate huge page! %ld\n", status);
				return 0;
			}
			printf("\tAllocated new huge page 0x%p\n", gb_page_addr);
			gb_page = (pdpe_1gb_t*)gb_page_addr;
			memset(gb_page, 0, PAGE_SIZE);

			gb_page->bits.present = true;
			gb_page->bits.writable = true;
			gb_page->bits.user_mode = false;
			// TODO(PT): Should be provided by the caller
			gb_page->bits.global = true;
			gb_page->bits.page_base = current_frame / PAGE_SIZE;
			printf("set huge page 0x%p\n", gb_page->phys_addr);

			page_directory_pointer_table[page_directory_idx].bits.present = true;
			page_directory_pointer_table[page_directory_idx].bits.writable = true;
			page_directory_pointer_table[page_directory_idx].bits.user_mode = false;
			page_directory_pointer_table[page_directory_idx].bits.page_dir_base = gb_page_addr / PAGE_SIZE;
			*/
			page_directory_pointer_table[page_directory_idx].bits.present = true;
			page_directory_pointer_table[page_directory_idx].bits.writable = true;
			page_directory_pointer_table[page_directory_idx].bits.user_mode = false;
			// TODO(PT): Should be provided by the caller
			page_directory_pointer_table[page_directory_idx].bits.global = true;
			// TODO(PT): Ensure current_frame lies on a GB boundary
			page_directory_pointer_table[page_directory_idx].bits.page_base = current_frame / gigabyte;
			page_directory_pointer_table[page_directory_idx].bits.must_be_one = 1;
			printf("\tSet huge page 0x%p\n", page_directory_pointer_table[page_directory_idx]);

			remaining_size -= gigabyte;
			current_frame += gigabyte;
		}
	}
}

uint64_t map_region(pml4e_t* page_mapping_level4, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start) {
	printf("map_region %p %p %p\n", vmem_start, vmem_size, phys_start);
	printf("map_region [phys 0x%p - 0x%p] to [virt 0x%p - 0x%p]\n", phys_start, phys_start + vmem_size - 1, vmem_start, vmem_start + vmem_size - 1);
	uint64_t remaining_size = vmem_size;
	uint64_t current_frame = phys_start;
	uint64_t current_page = vmem_start;

	int page_directory_pointer_table_idx = VMA_PML4E_IDX(vmem_start);
	printf("\tPDPE idx %ld\n", page_directory_pointer_table_idx);

	pdpe_t* page_directory_pointer_table = NULL;
	if (page_mapping_level4[page_directory_pointer_table_idx].bits.present) {
		// We've already created the necessary PDPT
		page_directory_pointer_table = (pdpe_t*)(page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base * PAGE_SIZE);
		printf("\tRe-using existing PDPT 0x%p\n", page_directory_pointer_table);
	}
	else {
		efi_physical_address_t page_directory_pointer_table_addr = 0;
		efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_directory_pointer_table_addr);
		if (EFI_ERROR(status)) {
			printf("\tFailed to allocate page directory pointer table! %ld\n", status);
			return 0;
		}
		page_directory_pointer_table = (pdpe_t*)page_directory_pointer_table_addr;
		memset(page_directory_pointer_table, 0, PAGE_SIZE);
		printf("\tAllocated new page directory pointer table at 0x%p\n", page_directory_pointer_table);

		page_mapping_level4[page_directory_pointer_table_idx].bits.present = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.writable = true;
		page_mapping_level4[page_directory_pointer_table_idx].bits.user_mode = false;
		page_mapping_level4[page_directory_pointer_table_idx].bits.page_dir_pointer_base = page_directory_pointer_table_addr / PAGE_SIZE;
	}

	uint64_t page_directories_needed = (remaining_size + (VMEM_IN_PDPE - 1)) / VMEM_IN_PDPE;
	page_directories_needed = min(page_directories_needed, PAGE_DIRECTORIES_IN_PAGE_DIRECTORY_POINTER_TABLE);
	uint64_t first_page_directory = VMA_PDPE_IDX(vmem_start);
	printf("\tvmem_size 0x%p page_dirs_needed %ld\n", vmem_size, page_directories_needed);

	for (int page_directory_iter_idx = 0; page_directory_iter_idx < page_directories_needed; page_directory_iter_idx++) {
	//for (int page_directory_idx = first_page_directory; page_directory_idx < first_page_directory + page_directories_needed; page_directory_idx++) {
		int page_directory_idx = page_directory_iter_idx + first_page_directory;
		printf("\tpage_directory_idx %ld\n", page_directory_idx);
		pde_t* page_directory = NULL;
		if (page_directory_pointer_table[page_directory_idx].bits.present) {
			// We've already created the necessary page directory
			page_directory = (pde_t*)(page_directory_pointer_table[page_directory_idx].bits.page_dir_base * PAGE_SIZE);
			printf("\tRe-using existing page directory 0x%p\n", page_directory);
		}
		else {
			efi_physical_address_t page_directory_addr = 0;
			efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_directory_addr);
			if (EFI_ERROR(status)) {
				printf("Failed to allocate page directory! %ld\n", status);
				return 0;
			}
			printf("\tAllocated new page directory 0x%p\n", page_directory_addr);
			page_directory = (pde_t*)page_directory_addr;
			memset(page_directory, 0, PAGE_SIZE);

			page_directory_pointer_table[page_directory_idx].bits.present = true;
			page_directory_pointer_table[page_directory_idx].bits.writable = true;
			page_directory_pointer_table[page_directory_idx].bits.user_mode = false;
			page_directory_pointer_table[page_directory_idx].bits.page_dir_base = page_directory_addr / PAGE_SIZE;
		}

		uint64_t page_tables_needed = (remaining_size + (VMEM_IN_PDE - 1)) / VMEM_IN_PDE;
		page_tables_needed = min(page_tables_needed, PAGE_TABLES_IN_PAGE_DIRECTORY);
		uint64_t first_page_table = VMA_PDE_IDX(vmem_start);

		for (int page_table_iter_idx = 0; page_table_iter_idx < page_tables_needed; page_table_iter_idx++) {
			int page_table_idx = page_table_iter_idx + first_page_table;
			//printf("\tpage_table_idx %ld\n", page_table_idx);
			pte_t* page_table = NULL;
			if (page_directory[page_table_idx].bits.present) {
				// We've already created the necessary page table
				page_table = (pte_t*)(page_directory[page_table_idx].bits.page_table_base * PAGE_SIZE);
				printf("\tRe-using existing page table 0x%p in page directory 0x%p\n", page_table, page_directory);
			}
			else {
				efi_physical_address_t page_table_addr = 0;
				efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_table_addr);
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
			pages_needed = min(pages_needed, PAGES_IN_PAGE_TABLE);
			uint64_t first_page = VMA_PTE_IDX(current_page);

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
	efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_mapping_level4_addr);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate PML4! %ld\n", status);
		return 0;
	}
	pml4e_t* page_mapping_level4 = (pml4e_t*)page_mapping_level4_addr;
	memset(page_mapping_level4, 0, PAGE_SIZE);

	// Identity map the first 4G of memory
	// PT: Each constant must individually be suffixed by LL, or else the 
	// product will be truncated to a uint32_t
	// Also, use %p instead of %16x
	map_region(page_mapping_level4, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0);

	// Map physical RAM 
	// TODO(PT): Move into the kernel?
	map_region_1gb_pages(page_mapping_level4, 0xFFFF800000000000, (1024LL * 1024LL * 1024LL * 64LL), 0x0);

	// Map the last 2G of memory to the first 2GB of memory
	//map_region(page_mapping_level4, 0xFFFFFFFF80000000, (1024LL * 1024LL * 1024LL * 2LL), 0x0);
	//map_region(page_mapping_level4, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0);

	printf("Allocated PML4 at 0x%p\n", page_mapping_level4);
	return page_mapping_level4;
}

pml4e_t* map(void) {
	printf("sizeof(pml4e_t) = %ld\n", sizeof(pml4e_t));
	printf("sizeof(pdpe_t) = %ld\n", sizeof(pdpe_t));
	printf("sizeof(pde_t) = %ld\n", sizeof(pde_t));
	printf("sizeof(pte_t) = %ld\n", sizeof(pte_t));
	if (sizeof(pml4e_t) != sizeof(uint64_t) ||
		sizeof(pdpe_t) != sizeof(uint64_t) ||
		sizeof(pde_t) != sizeof(uint64_t) ||
		sizeof(pte_t) != sizeof(uint64_t)) {
		printf("Wrong paging structure size!\n");
		return 0;
	}
	if (PAGE_TABLES_IN_PAGE_DIRECTORY != 512) {
		printf("Wrong number of page tables in page dir %ld!\n", PAGE_TABLES_IN_PAGE_DIRECTORY);
		return 0;
	}
	if (PAGES_IN_PAGE_TABLE != 512) {
		printf("Wrong number of page table entries in page table!\n");
		return 0;
	}

	efi_status_t status = 0;

	efi_physical_address_t page_directory_pointer_table_addr = 0;
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_directory_pointer_table_addr);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate page directory pointer table! %ld\n", status);
		return 0;
	}
	printf("Allocated page directory pointer table at 0x%p\n", page_directory_pointer_table_addr);
	pdpe_t* page_directory_pointer_table = (pdpe_t*)page_directory_pointer_table_addr;
	memset(page_directory_pointer_table, 0, PAGE_SIZE);

	for (int page_directory_idx = 0; page_directory_idx < 4; page_directory_idx++) {
		efi_physical_address_t page_directory_addr = 0;
		status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_directory_addr);
		if (EFI_ERROR(status)) {
			printf("Failed to allocate page directory! %ld\n", status);
			return 0;
		}
		//printf("Page directory 0x%p\n", page_directory_addr);
		pde_t* page_directory = (pde_t*)page_directory_addr;
		memset(page_directory, 0, PAGE_SIZE);

		for (int i = 0; i < PAGE_TABLES_IN_PAGE_DIRECTORY; i++) {
			efi_physical_address_t page_table_addr = 0;
			status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_table_addr);
			if (EFI_ERROR(status)) {
				printf("Failed to allocate page table! %ld\n", status);
				return 0;
			}
			pte_t* page_table = (pte_t*)page_table_addr;
			memset(page_table, 0, PAGE_SIZE);
			for (int j = 0; j < PAGES_IN_PAGE_TABLE; j++) {
				page_table[j].bits.present = true;
				page_table[j].bits.writable = true;
				page_table[j].bits.user_mode = false;
				//page_table[j].bits.page_base = ((j * PAGE_SIZE) + (i * PAGE_SIZE * 512));
				page_table[j].bits.page_base = (((j * PAGE_SIZE) + (i * PAGE_SIZE * 512) + (page_directory_idx * 512 * 512))) / PAGE_SIZE;
				page_table[j].bits.global_page = true;
				//printf("\tPage (%ld,%ld) phys = 0x%p\n", i, j, page_table[j].bits.page_base);
			}

			page_directory[i].bits.present = true;
			page_directory[i].bits.writable = true;
			page_directory[i].bits.user_mode = false;
			//printf("Set page table %ld addr to  0x%p\n", i, page_table_addr);
			page_directory[i].bits.page_table_base = page_table_addr / PAGE_SIZE;
		}

		page_directory_pointer_table[page_directory_idx].bits.present = true;
		page_directory_pointer_table[page_directory_idx].bits.writable = true;
		page_directory_pointer_table[page_directory_idx].bits.user_mode = false;
		page_directory_pointer_table[page_directory_idx].bits.page_dir_base = page_directory_addr / PAGE_SIZE;
	}

	efi_physical_address_t page_mapping_level4_addr = 0;
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &page_mapping_level4_addr);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate PML4! %ld\n", status);
		return 0;
	}
	pml4e_t* page_mapping_level4 = (pml4e_t*)page_mapping_level4_addr;
	memset(page_mapping_level4, 0, PAGE_SIZE);
	page_mapping_level4[0].bits.present = true;
	page_mapping_level4[0].bits.writable = true;
	page_mapping_level4[0].bits.user_mode = false;
	page_mapping_level4[0].bits.page_dir_pointer_base = page_directory_pointer_table_addr / PAGE_SIZE;

	printf("Allocated PML4 at 0x%p\n", page_mapping_level4);

	printf("CALLING...\n");
	//map_region(page_mapping_level4, 0xffffffff80000000, 0x00000000080000000, 0);
	//map_region(page_mapping_level4, 1, 2, 3);
	//map_region(page_mapping_level4, 0xffffffff80000000, 0x80000000, 0);
	//map_region(page_mapping_level4, 0xffffffff80000000, 0x80000000, 0);
	//map_region(page_mapping_level4, 0xffffffff80000000, 0x1000, 0);
	//map_region(page_mapping_level4, 0xffffffff80000000, 0x1000, 0x41000);
	//map_region(page_mapping_level4, 0xffffffff80001000, 0x2000, 0x414141000);

	//printf("Loading pml4 0x%p\n", page_mapping_level4_addr);
	/*
	asm("cli");
	asm volatile("movq %0, %%cr3" : : "r"(page_mapping_level4_addr));
	while (1) {}
	*/

	//hexdump(page_mapping_level4, PAGE_SIZE);

	// Identity map the first 4G of memory
	// This corresponds to the first page directory pointer entry
		//uint64_t page_dir_pointer_base:40;

	return page_mapping_level4;

	// 1024 pages in a table
	// 1024 tables in a page directory
	// 1024 page directories in a pml4e
	// 1024 pml4e's in pml4

	/*
	efi_physical_address_t segment_virt_base = phdr->p_vaddr;
	int segment_size = phdr->p_memsz;
	uint32_t page_count = (segment_size + (PAGE_SIZE-1)) & PAGE_MASK;
	printf("\tAllocating %ld pages for ELF segment: [0x%p - 0x%p]\n", page_count, segment_virt_base, segment_virt_base + segment_size);
	efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderCode, page_count, &segment_virt_base);
	if (EFI_ERROR(status)) {

		printf("Failed to map kernel segment at requested address\n");
		printf("Status: %ld\n", status);
		return 0;
	}
	printf("\tMapped [0x%p - 0x%p]\n", segment_virt_base, segment_virt_base + segment_size);
	*/
}

// TODO(PT): Expose the kernel ELF sections in the boot info, so the PMM can reserve them and we can store the symbol table
uint64_t kernel_map_elf(const char* kernel_filename, pml4e_t* vas_state) {
	FILE* kernel_file = fopen("\\EFI\\AXLE\\KERNEL.ELF", "r");
	if (!kernel_file) {
		printf("Unable to open KERNEL.ELF!\n");
		return 0;
	}

	// Read the kernel file contents into a buffer
	fseek(kernel_file, 0, SEEK_END);
	uint64_t kernel_size = ftell(kernel_file);
	fseek(kernel_file, 0, SEEK_SET);
	uint8_t* kernel_buf = malloc(kernel_size);
	if (!kernel_buf) {
		printf("Failed to allocate memory!\n");
		return 0;
	}
	fread(kernel_buf, kernel_size, 1, kernel_file);
	fclose(kernel_file);

	// Validate the ELF 
	Elf64_Ehdr* elf = (Elf64_Ehdr*)kernel_buf;
	if (memcmp(elf->e_ident, ELFMAG, SELFMAG)) {
		printf("ELF magic wrong!\n");
		return 0;
	}
	if (elf->e_ident[EI_CLASS] != ELFCLASS64) {
		printf("Not 64 bit\n");
		return 0;
	}
	if (elf->e_ident[EI_DATA] != ELFDATA2LSB) {
		printf("Not LSB (endianness?)\n");
		return 0;
	}
	if (elf->e_type != ET_EXEC) {
		printf("Not executable\n");
		return 0;
	}
	if (elf->e_machine != EM_MACH) {
		printf("Wrong arch\n");
		return 0;
	}
	if (elf->e_phnum <= 0) {
		printf("No program headers\n");
		return 0;
	}
	printf("Valid ELF!\n");

	// Load ELF segments
	for (uint64_t i = 0; i < elf->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(kernel_buf + elf->e_phoff + (i * elf->e_phentsize));
		printf("PH at 0x%p\n", phdr);
		if (phdr->p_type == PT_LOAD) {
			uint64_t bss_size = phdr->p_memsz - phdr->p_filesz;
			printf("ELF segment %p %d bytes (bss %d bytes)\n", phdr->p_vaddr, phdr->p_filesz, bss_size);

			efi_physical_address_t segment_phys_base = 0;
			int segment_size = phdr->p_memsz;
			int segment_size_page_padded = (segment_size + PAGE_SIZE - 1) & PAGE_MASK;
			int page_count = segment_size_page_padded / PAGE_SIZE;
			printf("\tAllocating %ld pages for ELF segment %ld\n", page_count, i);
			efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiLoaderCode, page_count, &segment_phys_base);
			if (EFI_ERROR(status)) {
				printf("Failed to map kernel segment at requested address\n");
				printf("Status: %ld\n", status);
				return 0;
			}

			memcpy(segment_phys_base, kernel_buf + phdr->p_offset, phdr->p_filesz);
			memset(segment_phys_base + phdr->p_filesz, 0, bss_size);

			printf("Mapping [phys 0x%p - 0x%p] - [virt 0x%p - 0x%p]\n", segment_phys_base, segment_phys_base + segment_size_page_padded - 1, phdr->p_vaddr, phdr->p_vaddr + segment_size_page_padded - 1);
			map_region(vas_state, phdr->p_vaddr, segment_size_page_padded, segment_phys_base);

			/*
			// Works at 1M
			memcpy(phdr->p_vaddr, kernel_buf + phdr->p_offset, phdr->p_filesz);
			memset(phdr->p_vaddr + phdr->p_filesz, 0, bss_size);
			*/

			//printf("\tMapped [0x%p - 0x%p]\n", segment_virt_base, segment_virt_base + segment_size);

			//map_region(page_mapping_level4, 0xffffffff80000000, 0x80000000, 0);
			//map_region(page_mapping_level4, 0xffffffff80001000, 0x2000, 0x414141000);
			//memcpy((void*)phdr->p_vaddr, kernel_buf + phdr->p_offset, phdr->p_filesz);
			//memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, bss_size); 
		}
	}

	uintptr_t kernel_entry_point = elf->e_entry;
	free(kernel_buf);
	return kernel_entry_point;
}

#define PAGE_SIZE 0x1000

bool initrd_map(const char* initrd_path, uint64_t* out_base, uint64_t* out_size) {
	FILE* initrd_file = fopen(initrd_path, "r");
	if (!initrd_file) {
		printf("Failed to open initrd!\n");
		return false;
	}
	fseek(initrd_file, 0, SEEK_END);
	uint64_t initrd_size = ftell(initrd_file);
	fseek(initrd_file, 0, SEEK_SET);
	uint64_t initrd_page_count = (initrd_size / PAGE_SIZE) + 1;
	printf("Initrd size 0x%p, page count %ld\n", initrd_size, initrd_page_count);
	efi_physical_address_t initrd_buf = 0;
	efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, initrd_page_count, &initrd_buf);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate memory for initrd! %ld\n", status);
		return false;
	}
	printf("Allocated buffer for initrd: 0x%p\n", initrd_buf);
	fread(initrd_buf, initrd_size, 1, initrd_file);
	printf("Mapped initrd\n");
	fclose(initrd_file);

	*out_base = initrd_buf;
	*out_size = initrd_size;

	return true;
}

int main(int argc, char** argv) {
	ST->ConOut->ClearScreen(ST->ConOut);
	printf("axle OS bootloader init...\n");

	// Step 1: Allocate the buffer we'll use to pass all the info to the kernel
	// We've got to do this before reading the memory map, as allocations modify
	// the memory map.
	axle_boot_info_t* boot_info = calloc(1, sizeof(axle_boot_info_t));
	pml4e_t* page_mapping_level4 = map2();
	boot_info->debug_pml4 = page_mapping_level4;

	// Step 2: Map the kernel ELF into memory
	uint64_t kernel_entry_point = kernel_map_elf("\\EFI\\AXLE\\KERNEL.ELF", page_mapping_level4);
	if (!kernel_entry_point) {
		printf("Failed to map kernel\n");
		return 0;
	}

	// Step 3: Map the initrd into memory
	if (!initrd_map("\\EFI\\AXLE\\INITRD.IMG", &boot_info->initrd_base, &boot_info->initrd_size)) {
		printf("Failed to map initrd!\n");
		return 0;
	}

	// Step 3: Select a graphics mode
	efi_gop_t* gop = NULL;
	efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	efi_status_t status = BS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
	if (EFI_ERROR(status)) {
		printf("Failed to locate GOP!\n");
		return 0;
	}

	uint64_t gop_mode_info_size = 0;
	efi_gop_mode_info_t* gop_mode_info = NULL;
	uint64_t best_mode = gop->Mode->Mode;
	// Desired aspect ratio is 16:9
	double desired_aspect_ratio = 16.0 / 9.0;
	double min_distance = 1000000.0;
	printf("Desired aspect ratio: %p\n", desired_aspect_ratio);
	
	for (uint64_t i = gop->Mode->Mode; i < gop->Mode->MaxMode; i++) {
		gop->QueryMode(gop, i,  &gop_mode_info_size,  &gop_mode_info);
		printf("Mode %ld: %ldx%ld, %ld bpp\n", i, gop_mode_info->HorizontalResolution, gop_mode_info->VerticalResolution, gop_mode_info->PixelFormat);
		double aspect_ratio = gop_mode_info->HorizontalResolution / (double)gop_mode_info->VerticalResolution;
		if (abs(desired_aspect_ratio - aspect_ratio) < min_distance) {
			printf("\tFound new best aspect ratio match!\n");
			best_mode = i;
			min_distance = abs(desired_aspect_ratio - aspect_ratio);
		}
	}
	gop->QueryMode(gop, best_mode,  &gop_mode_info_size,  &gop_mode_info);
	gop->SetMode(gop, best_mode);
	printf("Selected Mode %ld: %ldx%ld, %ld bpp\n", best_mode, gop_mode_info->HorizontalResolution, gop_mode_info->VerticalResolution, gop_mode_info->PixelFormat);
	boot_info->framebuffer_base = gop->Mode->FrameBufferBase;
	boot_info->framebuffer_width = gop->Mode->Information->HorizontalResolution;
	boot_info->framebuffer_height = gop->Mode->Information->VerticalResolution;
	boot_info->framebuffer_bytes_per_pixel = 4;

	// Step 5: Read the memory map
	// Calling GetMemoryMap with an invalid buffer allows us to read info on 
	// how much memory we'll need to store the memory map.
	uint64_t memory_map_size = 0; 
	efi_memory_descriptor_t* memory_descriptors = NULL;
	uint64_t memory_map_key = 0;
	uint64_t memory_descriptor_size = 0;
	uint32_t memory_descriptor_version = 0;
	status = BS->GetMemoryMap(
		&memory_map_size,
		NULL,
		&memory_map_key,
		&memory_descriptor_size,
		&memory_descriptor_version
	);
	// This first call should never succeed as we're using it to determine the needed buffer space
	if (status != EFI_BUFFER_TOO_SMALL || !memory_map_size) {
		printf("Expected buffer to be too small...\n");
		return 0;
	}

	// Now we know how big the memory map needs to be.
	printf("Set memory map size to: %p\n", memory_map_size);
	printf("Memory descriptors: %p\n", memory_descriptors);
	printf("Memory map key: %p\n", memory_map_key);
	printf("Memory descriptor size: %p\n", memory_descriptor_size);
	printf("Memory descriptor version: %p\n", memory_descriptor_version);

	// Allocate the buffer for the memory descriptors, 
	// but this will change the memory map and may increase its size!
	// Reserve some extra space just in case
	memory_map_size += (memory_descriptor_size * 4);
	printf("Reserved extra memory map space, buffer size is now %p\n", memory_map_size);
	// https://uefi.org/sites/default/files/resources/UEFI_Spec_2_8_final.pdf
	memory_descriptors = malloc(memory_map_size);

	status = BS->GetMemoryMap(
		&memory_map_size,
		memory_descriptors,
		&memory_map_key,
		&memory_descriptor_size,
		NULL
	);
	// The buffer should have been large enough...
	if (EFI_ERROR(status)) {
		printf("Error reading memory map!\n");
		return 0;
	}

	// Note that the memory layout must be identical between 
	// efi_memory_descriptor_t and axle_efi_memory_descriptor_t, since we cast it here
	if (sizeof(efi_memory_descriptor_t) != sizeof(axle_efi_memory_descriptor_t)) {
		printf("efi_memory_descriptor_t and axle_efi_memory_descriptor_t were different sizes!\n");
		return 0;
	}
	boot_info->memory_descriptors = (axle_efi_memory_descriptor_t*)memory_descriptors;
	boot_info->memory_descriptor_size = memory_descriptor_size;
	boot_info->memory_map_size = memory_map_size;

	// Finally, exit UEFI-land and jump to the kernel
	printf("Jumping to kernel entry point at %p\n", kernel_entry_point);
	if (exit_bs()) {
		printf("Failed to exit boot services!\n");
		while (1) {}
		return 0;
	}
	/*
	void go(uint64_t, uint64_t);
	go(page_mapping_level4, kernel_entry_point);
	printf("Blah!\n");
	*/
	 //asm("cli");
	asm volatile("movq %0, %%cr3" : : "r"(page_mapping_level4));

	//while (1) {}

	// This should never return...
	(*((int(* __attribute__((sysv_abi)))(axle_boot_info_t*))(kernel_entry_point)))(boot_info);

	// If we got here, the kernel returned control to the bootloader
	// This should never happen with a well-behaved kernel...
	return 0;
}
