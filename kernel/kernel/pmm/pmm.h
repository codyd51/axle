#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <kernel/address_space.h>
#include "pmm_int.h"

pmm_state_t* pmm_get(void);
void pmm_init(void);

uintptr_t pmm_alloc(void);
void pmm_alloc_address(uintptr_t address);
uintptr_t pmm_alloc_continuous_range(uintptr_t size);

void pmm_free(uintptr_t frame_addr);

void pmm_dump(void);
uintptr_t pmm_allocated_memory(void);

bool pmm_is_address_allocated(uintptr_t address);
bool pmm_is_frame_general_purpose(uintptr_t address);

void pmm_test(void);

void pmm_push_allocatable_frame(uint64_t frame_addr);

#endif
