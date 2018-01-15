#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <kernel/address_space.h>

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

typedef struct address_space_page_bitset {
    //bitset where each bit refers to a 4kb frame
    //available_frames[0] & (1 << 0) == reference to the first frame in the address space, 0x0000 to 0x1000
    uint32_t set[ADDRESS_SPACE_BITMAP_SIZE];
} address_space_page_bitmap_t;

//VMM memory space should have bitset of frames mapped in
typedef struct vmm_state {
    //if a frame's bit is set, it is general-purpose RAM which can be allocated to the virtual memory manager
    //else, the frame is reserved by the system and should not be touched by PMM
    address_space_page_bitmap_t system_accessible_pages;
    //if a frame's bit is set, it has been allocated by the PMM and is currently in use
    //else, it is not in use and may be allocated by the PMM
    address_space_page_bitmap_t allocation_state;
} vmm_state_t;

void vmm_init(void);

#endif
