#ifndef PAGING_H
#define PAGING_H

#include <uefi.h>

#define PAGE_SIZE 0x1000
#define ROUND_TO_NEXT_PAGE(val) ((val + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

typedef union pml4e pml4e_t;

uint64_t map_region_1gb_pages(pml4e_t* page_mapping_level4, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start);
uint64_t map_region_4k_pages(pml4e_t* page_mapping_level4, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start);
pml4e_t* map2(void);

#endif
