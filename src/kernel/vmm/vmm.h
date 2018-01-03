#ifndef VMM_H
#define VMM_H

#include <kernel/pmm/pmm.h>

// Constants between pages and frames are, by definition, equivelent
#define PAGING_PAGE_SIZE PAGING_FRAME_SIZE
#define ADDRESS_SPACE_PAGE_NUM ADDRESS_SPACE_FRAME_NUM
typedef address_space_frame_bitset_t address_space_page_bitset_t;

#endif