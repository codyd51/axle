#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <kernel/address_space.h>
#include <kernel/address_space_bitmap.h>
#include <kernel/util/spinlock/spinlock.h>

struct pmm_state {
    //if a frame's bit is set, it is general-purpose RAM which can be allocated to the virtual memory manager
    //else, the frame is reserved by the system and should not be touched by PMM
    address_space_frame_bitmap_t system_accessible_frames;
    //if a frame's bit is set, it has been allocated by the PMM and is currently in use
    //else, it is not in use and may be allocated by the PMM
    address_space_frame_bitmap_t allocation_state;
    spinlock_t lock;
} __attribute__((aligned(PAGE_SIZE)));
typedef struct pmm_state pmm_state_t;

pmm_state_t* pmm_get(void);
void pmm_init(void);

uint32_t pmm_alloc(void);
void pmm_alloc_address(uint32_t address);
uint32_t pmm_alloc_continuous_range(uint32_t size);

void pmm_free(uint32_t frame_addr);

void pmm_dump(void);

#endif
