#ifndef ADDRESS_SPACE_BITMAP_H
#define ADDRESS_SPACE_BITMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/util/mutex/mutex.h>

//1048576 frames / 32-bit frame bitsets = 32,768 32-bit
//frame bitsets needed to cover entire address space
//32 frames total
#define ADDRESS_SPACE_BITMAP_SIZE 0x8000
#define PAGE_SIZE 0x1000

//number of bool values stored by one entry in the address_space_frame_bitset.set array.
#define BITS_PER_BITMAP_ENTRY (sizeof(uint32_t) * BITS_PER_BYTE)
//translate a {word index, bit-within-word index} pair to
//the absolute bit index into the bitset
#define BITMAP_BIT_INDEX(x, y) ((x) * BITS_PER_BITMAP_ENTRY + (y))
// translate an address into the index of the 32-bit word that contains its corresponding bit
#define BITMAP_WORD_INDEX(addr) (addr / (PAGE_SIZE * BITS_PER_BITMAP_ENTRY))
// translate an address into the bit-index within 32-bit word that contains its corresponding bit
#define BITMAP_INDEX_WITHIN_WORD(addr) ((addr % (PAGE_SIZE * BITS_PER_BITMAP_ENTRY) / PAGE_SIZE))

struct address_space_frame_bitmap {
    // bitset where each bit refers to a 4kb frame
    // available_frames[0] & (1 << 0) == reference to the first frame in the address space, 0x0000 to 0x1000
    uint32_t set[ADDRESS_SPACE_BITMAP_SIZE];
    // Lock to ensure the bitset is always modified in an exclusive fashion
    lock_t lock;
} __attribute__((aligned(PAGE_SIZE)));

typedef struct address_space_frame_bitmap address_space_frame_bitmap_t;
typedef address_space_frame_bitmap_t address_space_page_bitmap_t;

void addr_space_bitmap_set_address(address_space_frame_bitmap_t* bitmap, uint32_t address);
void addr_space_bitmap_set_range(address_space_frame_bitmap_t* bitmap, uint32_t start_address, uint32_t size);
void addr_space_bitmap_unset_address(address_space_frame_bitmap_t* bitmap, uint32_t address);
bool addr_space_bitmap_check_address(address_space_frame_bitmap_t* bitmap, uint32_t address);

void addr_space_bitmap_dump_set_ranges(address_space_frame_bitmap_t* bitmap);

#endif
