#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#include <stdint.h>

// Constants between pages and frames are, by definition, equivelent
#define PAGING_FRAME_SIZE   0x1000
// Constants between pages and frames are, by definition, equivelent
#define PAGING_PAGE_SIZE   0x1000

//4GB address space / 4kb frame size = 1048576 = 0x100000
#define ADDRESS_SPACE_FRAME_NUM 0x100000

//4GB address space / 4kb frame size = 1048576 = 0x100000
#define ADDRESS_SPACE_PAGE_NUM 0x100000

#define PAGING_FRAME_MASK 0xFFFFF000
#define PAGE_TABLE_ENTRY_MASK 0xFFFFF000
#define PAGE_TABLE_FLAG_BITS_MASK 0x00000FFF
#define PAGE_DIRECTORY_ENTRY_MASK 0xFFFFF000
#define PAGE_FLAG_BITS_MASK 0x00000FFF

uint32_t addr_space_frame_floor(uint32_t addr);
uint32_t addr_space_frame_ceil(uint32_t addr);

uint32_t addr_space_page_floor(uint32_t addr);
uint32_t addr_space_page_ceil(uint32_t addr);

#endif
