#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <kernel/address_space.h>
#include <kernel/address_space_bitmap.h>

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
} vmm_page_directory_t;

//VMM memory space should have bitset of frames mapped in
typedef struct vmm_state {
    address_space_page_bitmap_t allocated_pages;
} vmm_state_t;

void vmm_init(void);

#endif
