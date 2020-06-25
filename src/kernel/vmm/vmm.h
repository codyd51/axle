#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>
#include <std/common.h>
#include <std/common.h>

#include <kernel/address_space.h>
#include <kernel/address_space_bitmap.h>

#include <kernel/interrupts/interrupts.h>

#define PAGE_PRESENT_FLAG (1 << 0)
#define PAGE_NOT_PRESENT_FLAG (0 << 0)


#define PAGE_READ_WRITE_FLAG (1 << 1)
#define PAGE_WRITE_ONLY_FLAG (0 << 1)

#define PAGE_USER_MODE_FLAG (1 << 2)
#define PAGE_KERNEL_ONLY_FLAG (0 << 2)

#define TABLES_IN_PAGE_DIRECTORY 1024
#define ACTIVE_PAGE_DIRECTORY_HEAD 0xFFFFF000
#define ACTIVE_PAGE_TABLE_ARRAY_HEAD 0xFFC00000
#define ACTIVE_PAGE_BITMAP_HEAD (ACTIVE_PAGE_TABLE_ARRAY_HEAD - sizeof(address_space_page_bitmap_t))

// TODO(PT): Flat structure for physically placed vas_state.
// Virtual structure with pointers to ACTIVE_* or otherwise mapped structures

typedef struct vmm_page {
    uint32_t present    :  1; //page present in memory
    uint32_t writable   :  1; //read-only if clear, readwrite if set
    uint32_t user_mode  :  1; //kernel level only if clear
    uint32_t accessed   :  1; //has page been accessed since last refresh?
    uint32_t dirty      :  1; //has page been written to since last refresh?
    uint32_t unused     :  7; //unused/reserved bits
    uint32_t frame_idx  : 20; //frame index, shifted right 12 bits. The actual frame address is this value * PAGING_FRAME_SIZE
} vmm_page_t;

typedef struct vmm_page_table {
    vmm_page_t pages[1024];
} vmm_page_table_t;

typedef struct vmm_page_directory {
    vmm_page_table_t* table_pointers[1024];
	address_space_page_bitmap_t allocated_pages;
} vmm_page_directory_t;

typedef struct vmm_state {
    address_space_page_bitmap_t allocated_pages;
} vmm_state_t;

typedef struct page_directory {
	//array of pointers to pagetables
	vmm_page_t* tables[1024];

	//array of pointers to pagetables above, but give their *physical*
	//location, for loading into CR3 reg
	uint32_t tablesPhysical[1024];

	//physical addr of tablesPhysical.
	//needed once kernel heap is allocated and
	//directory may be in a different location in virtual memory
	uint32_t physicalAddr;
} page_directory_t;

//page_t* vmm_get_page_for_virtual_address(vmm_pdir_t* dir, uint32_t virt_addr);

//void vmm_map_page_to_frame(page_t* page, uint32_t frame_addr);

void vmm_map_region(vmm_page_directory_t* vmm_dir, uint32_t start_addr, uint32_t size);
void vmm_identity_map_region(vmm_page_directory_t* vmm_dir, uint32_t start_addr, uint32_t size);

void vmm_dump(vmm_page_directory_t* dir);

void vmm_init(void);
bool vmm_is_active();

void vmm_load_pdir(vmm_page_directory_t* dir, bool pause_interrupts);
uint32_t vmm_active_pdir();

uint32_t vmm_get_phys_for_virt(uint32_t virtualaddr);
//void vmm_map_virt_to_phys(vmm_pdir_t* dir, uint32_t page_addr, uint32_t frame_addr, uint16_t flags);
//void vmm_map_virt(vmm_pdir_t* dir, uint32_t page_addr, uint16_t flags);

vmm_page_directory_t* vmm_clone_pdir(vmm_page_directory_t* source_vmm_dir);

uint32_t vmm_alloc_page(vmm_page_directory_t* vmm_dir, bool readwrite);
uint32_t vmm_alloc_continuous_range(vmm_page_directory_t* vmm_dir, uint32_t size, bool readwrite);

#endif
