#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <kernel/address_space.h>

typedef struct address_space_frame_bitset {
    //bitset where each bit refers to a 4kb frame
    //available_frames[0] & (1 << 0) == reference to the first frame in the address space, 0x0000 to 0x1000
    uint32_t set[ADDRESS_SPACE_BITMAP_SIZE];
} address_space_frame_bitmap_t;

typedef struct pmm_state {
    //if a frame's bit is set, it is general-purpose RAM which can be allocated to the virtual memory manager
    //else, the frame is reserved by the system and should not be touched by PMM
    address_space_frame_bitmap_t system_accessible_frames;
    //if a frame's bit is set, it has been allocated by the PMM and is currently in use
    //else, it is not in use and may be allocated by the PMM
    address_space_frame_bitmap_t allocation_state;
} pmm_state_t;

pmm_state_t* pmm_get(void);
void pmm_init(void);

uint32_t pmm_alloc(void);
void pmm_free(uint32_t frame_addr);

void pmm_dump(void);

#endif
