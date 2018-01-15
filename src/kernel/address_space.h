#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

// Constants between pages and frames are, by definition, equivelent
#define PAGING_FRAME_SIZE   0x1000
// Constants between pages and frames are, by definition, equivelent
#define PAGING_PAGE_SIZE   0x1000

//4GB address space / 4kb frame size = 1048576 = 0x100000
#define ADDRESS_SPACE_FRAME_NUM 0x100000

//4GB address space / 4kb frame size = 1048576 = 0x100000
#define ADDRESS_SPACE_PAGE_NUM 0x100000

//1048576 frames / 32-bit frame bitsets = 32,768 32-bit
//frame bitsets needed to cover entire address space
#define ADDRESS_SPACE_BITMAP_SIZE 0x8000

#define PAGING_FRAME_MASK 0xFFFFF000

#endif
