#include "address_space_bitmap.h"
#include "address_space.h"
#include <std/common.h>
#include <kernel/assert.h>

void bitmap_set(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset) {
    //TODO(PT) validate index and offset
    bitmap->set[index] = bitmap->set[index] | (1 << offset);
    invlpg((uint32_t)&bitmap->set[index] & 0xfffff000);
}

void bitmap_unset(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset) {
    bitmap->set[index] = bitmap->set[index] & ~(1 << offset);
    invlpg((uint32_t)&bitmap->set[index] & 0xfffff000);
}

bool bitmap_check(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset) {
    return bitmap->set[index] & (1 << offset);
}

static uint32_t bitmap_index_of_first_bit_with_value(address_space_frame_bitmap_t* bitmap, bool desired_value) {
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t bitmap_entry = bitmap->set[i];
        //check if it's totally full (every bit is already set)
        if (!(~bitmap_entry)) {
            continue;
        }
        //iterate the bits
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            bool bit = bitmap_entry & (1 << j);
            if (bit == desired_value) {
                //found bit we're looking for
                return BITMAP_BIT_INDEX(i, j);
            }
        }
    }
    //no free bits found
    //ENOMEM
    panic("bitmap_index_of_first_bit_with_value found no available bits");
}

static uint32_t bitmap_index_of_first_set_bit(address_space_frame_bitmap_t* bitmap) {
    return bitmap_index_of_first_bit_with_value(bitmap, true);
}

static uint32_t bitmap_index_of_first_unset_bit(address_space_frame_bitmap_t* bitmap) {
    return bitmap_index_of_first_bit_with_value(bitmap, false);
}


static uint32_t addr_to_bitmap_index(uint32_t address) {
    return (address / PAGING_FRAME_SIZE) / BITS_PER_BITMAP_ENTRY;
}

static uint32_t addr_to_bitmap_entry_offset(uint32_t address) {
    return (address / PAGING_FRAME_SIZE) % BITS_PER_BITMAP_ENTRY;
}

static void _addr_space_bitmap_set_address_unlocked(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    uint32_t index = addr_to_bitmap_index(address);
    uint32_t offset = addr_to_bitmap_entry_offset(address);
    bitmap_set(bitmap, index, offset);
}

static void _addr_space_bitmap_unset_address_unlocked(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    uint32_t index = addr_to_bitmap_index(address);
    uint32_t offset = addr_to_bitmap_entry_offset(address);
    bitmap_unset(bitmap, index, offset);
}

static void _addr_space_bitmap_set_range_unlocked(address_space_frame_bitmap_t* bitmap, uint32_t start_address, uint32_t size) {
    if (start_address & PAGE_FLAG_BITS_MASK) panic("start_address is not page-aligned");
    if (size & PAGE_FLAG_BITS_MASK) panic("size is not page-aligned");
    for (uint32_t addr = start_address; addr < start_address + size; addr += PAGING_PAGE_SIZE) {
        _addr_space_bitmap_set_address_unlocked(bitmap, addr);
    }
}

static void _addr_space_bitmap_unset_range_unlocked(address_space_frame_bitmap_t* bitmap, uint32_t start_address, uint32_t size) {
    if (start_address & PAGE_FLAG_BITS_MASK) panic("start_address is not page-aligned");
    if (size & PAGE_FLAG_BITS_MASK) panic("size is not page-aligned");
    for (uint32_t addr = start_address; addr < start_address + size; addr += PAGING_PAGE_SIZE) {
        _addr_space_bitmap_unset_address_unlocked(bitmap, addr);
    }
}

static bool _addr_space_bitmap_check_address_unlocked(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    uint32_t index = addr_to_bitmap_index(address);
    uint32_t offset = addr_to_bitmap_entry_offset(address);

    bool ret = bitmap_check(bitmap, index, offset);
    return ret;
}

void addr_space_bitmap_dump_set_ranges(address_space_frame_bitmap_t* bitmap) {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    bool in_range = false;
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t entry = bitmap->set[i];
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            if (!in_range) {
                //if we encounter an on bit, a on region begins here
                if (entry & (1 << j)) {
                    range_start = BITMAP_BIT_INDEX(i, j) * PAGING_FRAME_SIZE;
                    in_range = true;
                    continue;
                }
            }
            else {
                //if we're in an on region and we encounter an off bit, an on region stops here
                if (!(entry & (1 << j))) {
                    range_end = BITMAP_BIT_INDEX(i, j) * PAGING_FRAME_SIZE;
                    in_range = false;
                    printf("\t\tbmp 0x%08x - 0x%08x\n", range_start, range_end);
                }
            }
        }
    }
}

/*
 * Public API wrappers
 * Enforces mutex on reads and writes
 */

void addr_space_bitmap_set_address(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    lock(&bitmap->lock);
    _addr_space_bitmap_set_address_unlocked(bitmap, address);
    unlock(&bitmap->lock);
}

void addr_space_bitmap_unset_address(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    lock(&bitmap->lock);
    _addr_space_bitmap_unset_address_unlocked(bitmap, address);
    unlock(&bitmap->lock);
}

void addr_space_bitmap_set_range(address_space_frame_bitmap_t* bitmap, uint32_t start_address, uint32_t size) {
    lock(&bitmap->lock);
    _addr_space_bitmap_set_range_unlocked(bitmap, start_address, size);
    unlock(&bitmap->lock);
}

void addr_space_bitmap_unset_range(address_space_frame_bitmap_t* bitmap, uint32_t start_address, uint32_t size) {
    lock(&bitmap->lock);
    _addr_space_bitmap_unset_range_unlocked(bitmap, start_address, size);
    unlock(&bitmap->lock);
}

bool addr_space_bitmap_check_address(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    lock(&bitmap->lock);
    bool ret = _addr_space_bitmap_check_address_unlocked(bitmap, address);
    unlock(&bitmap->lock);
    return ret;
}
