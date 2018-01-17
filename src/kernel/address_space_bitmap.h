#ifndef ADDRESS_SPACE_BITMAP_H
#define ADDRESS_SPACE_BITMAP_H

#include <stdint.h>
#include <stdbool.h>

//1048576 frames / 32-bit frame bitsets = 32,768 32-bit
//frame bitsets needed to cover entire address space
#define ADDRESS_SPACE_BITMAP_SIZE 0x8000

#define BITS_PER_BITMAP_ENTRY (sizeof(uint32_t) * BITS_PER_BYTE)
#define BITMAP_INDEX(x, y) ((x) * BITS_PER_BITMAP_ENTRY + (y))

typedef struct address_space_frame_bitset {
    //bitset where each bit refers to a 4kb frame
    //available_frames[0] & (1 << 0) == reference to the first frame in the address space, 0x0000 to 0x1000
    uint32_t set[ADDRESS_SPACE_BITMAP_SIZE];
} address_space_frame_bitmap_t;
typedef address_space_frame_bitmap_t address_space_page_bitmap_t;


void bitmap_set(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset);
void bitmap_unset(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset);
bool bitmap_check(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset);

uint32_t bitmap_index_of_first_set_bit(address_space_frame_bitmap_t* bitmap);
uint32_t bitmap_index_of_first_unset_bit(address_space_frame_bitmap_t* bitmap);

void addr_space_bitmap_set_address(address_space_frame_bitmap_t* bitmap, uint32_t address);
void addr_space_bitmap_unset_address(address_space_frame_bitmap_t* bitmap, uint32_t address);
bool addr_space_bitmap_check_address(address_space_frame_bitmap_t* bitmap, uint32_t address);


#endif
