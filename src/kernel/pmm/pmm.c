#include <stdbool.h>
#include <kernel/assert.h>
#include <kernel/boot_info.h>

#include <std/common.h>

#include "pmm.h"

#define BITS_PER_BITMAP_ENTRY (sizeof(uint32_t) * BITS_PER_BYTE)

#define BITMAP_INDEX(x, y) ((x) * BITS_PER_BITMAP_ENTRY + (y))

static void bitmap_set(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset) {
    //TODO(PT) validate index and offset
    bitmap->set[index] = bitmap->set[index] | (1 << offset);
}

static void bitmap_unset(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset) {
    bitmap->set[index] = bitmap->set[index] & ~(1 << offset);
}

static bool bitmap_check(address_space_frame_bitmap_t* bitmap, uint32_t index, uint32_t offset) {
    return bitmap->set[index] & (1 << offset);
}

static uint32_t address_to_bitmap_index(uint32_t address) {
    return (address / PAGING_FRAME_SIZE) / BITS_PER_BITMAP_ENTRY;
}

static uint32_t address_to_bitmap_entry_offset(uint32_t address) {
    return (address / PAGING_FRAME_SIZE) % BITS_PER_BITMAP_ENTRY;
}

static void bitmap_set_address(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    uint32_t index = address_to_bitmap_index(address);
    uint32_t offset = address_to_bitmap_entry_offset(address);
    bitmap_set(bitmap, index, offset);
}

static void bitmap_unset_address(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    uint32_t index = address_to_bitmap_index(address);
    uint32_t offset = address_to_bitmap_entry_offset(address);
    bitmap_unset(bitmap, index, offset);
}

static bool bitmap_check_address(address_space_frame_bitmap_t* bitmap, uint32_t address) {
    uint32_t index = address_to_bitmap_index(address);
    uint32_t offset = address_to_bitmap_entry_offset(address);
    return bitmap_check(bitmap, index, offset);
}

static uint32_t index_of_first_bit_with_value_in_bitmap(address_space_frame_bitmap_t* bitmap, bool desired_value) {
    pmm_state_t* pmm = pmm_get();
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t bitmap = pmm->system_accessible_frames.set[i];
        //check if it's totally full (every bit is already set)
        if (!(~bitmap)) {
            continue;
        }
        //iterate the bits
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            bool bit = bitmap & (1 << j);
            if (bit == desired_value) {
                //found bit we're looking for
                return BITMAP_INDEX(i, j);
            }
        }
    }
    //no free bits found
    //ENOMEM
    panic("PMM has run out of frames");
}

static uint32_t first_usable_pmm_index(pmm_state_t* pmm) {
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t system_frames_entry = pmm->system_accessible_frames.set[i];
        //skip early if either of these entries are unusable
        //have all of these frames been reserved by system? (no bits set)
        if (!system_frames_entry) {
            continue;
        }

        uint32_t pmm_frames_entry = pmm->allocation_state.set[i];
        //have all these frames already been allocated by PMM? (all bits set)
        if (!(~pmm_frames_entry)) {
            continue;
        }

        //test each bit in both bitmaps
        //if we find a bit on in both bitmaps, we've found a usable index
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            //is this frame reserved by the system? (bit is off)
            if (!(system_frames_entry & (1 << j))) {
                continue;
            }
            //is this frame already allocated by PMM? (bit is on)
            if (pmm_frames_entry & (1 << j)) {
                continue;
            }
            //we found a bit which was on in both arrays!
            return BITMAP_INDEX(i, j);
        }
    }
    panic("first_usable_pmm_index() found nothing!");
    return 0;
} 

static void set_memory_region(address_space_frame_bitmap_t* bitmap, uint32_t region_start_addr, uint32_t region_len) {
    if (region_start_addr % PAGING_FRAME_SIZE) {
        panic("region_start_addr wasn't page aligned");
    }
    if (region_len % PAGING_FRAME_SIZE) {
        panic("region_len wasn't frame aligned");
    }

    uint32_t frames_in_region = region_len / PAGING_FRAME_SIZE;
    for (int i = 0; i < frames_in_region; i++) {
        uint32_t frame = region_start_addr + (i * PAGING_FRAME_SIZE);
        bitmap_set_address(bitmap, frame);
    }
}

static void unset_memory_region(address_space_frame_bitmap_t* bitmap, uint32_t region_start_addr, uint32_t region_len) {
    if (region_start_addr % PAGING_FRAME_SIZE) {
        panic("region_start_addr wasn't page aligned");
    }
    if (region_len % PAGING_FRAME_SIZE) {
        panic("region_len wasn't frame aligned");
    }

    uint32_t frames_in_region = region_len / PAGING_FRAME_SIZE;
    for (int i = 0; i < frames_in_region; i++) {
        uint32_t frame = region_start_addr + (i * PAGING_FRAME_SIZE);
        bitmap_unset_address(bitmap, frame);
    }
}

pmm_state_t* pmm_get(void) {
    static pmm_state_t state = {0};
    return &state;
}

static void bitmap_dump_set_ranges(address_space_frame_bitmap_t* bitmap) {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    bool in_range = false;
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t entry = bitmap->set[i];
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            if (!in_range) {
                //if we encounter an on bit, a on region begins here
                if (entry & (1 << j)) {
                    range_start = BITMAP_INDEX(i, j) * PAGING_FRAME_SIZE;
                    in_range = true;
                    continue;
                }
            }
            else {
                //if we're in an on region and we encounter an off bit, an on region stops here
                if (!(entry & (1 << j))) {
                    range_end = BITMAP_INDEX(i, j) * PAGING_FRAME_SIZE;
                    in_range = false;
                    printf("\t\t0x%08x - 0x%08x\n", range_start, range_end);
                }
            }
        }
    }
}

void pmm_dump(void) {
    pmm_state_t* pmm = pmm_get();
    printf("Physical memory manager state:\n");
    printf("\tSystem accessible frames (ranges are allocatable):\n");
    bitmap_dump_set_ranges(&pmm->system_accessible_frames);
    printf("\tFrame allocation state (ranges are allocated):\n");
    bitmap_dump_set_ranges(&pmm->allocation_state);
}

static uint32_t frame_floor(uint32_t addr) {
    uint32_t orig=addr;
    if (addr & ~PAGING_FRAME_MASK) {
        addr &= PAGING_FRAME_MASK;
        return addr - PAGING_FRAME_SIZE;
    }
    return addr;

}

static uint32_t frame_ceil(uint32_t addr) {
    if (addr & ~PAGING_FRAME_MASK) {
        addr &= PAGING_FRAME_MASK;
        return addr + PAGING_FRAME_SIZE;
    }
    return addr;
}

void pmm_init() {
    pmm_state_t* pmm = pmm_get();
    memset(pmm, 0, sizeof(pmm_state_t));

    boot_info_t* info = boot_info_get();
    //mark usable sections of the address space
    for (int i = 0; i < info->mem_region_count; i++) {
        physical_memory_region_t region = info->mem_regions[i];
        if (region.type != REGION_USABLE) {
            continue;
        }
        //mask to frame size
        //this cuts off a bit of usable memory but we'll only lose a few frames at most
        uint32_t addr = frame_ceil(region.addr);
        uint32_t len = frame_floor(region.len);
        set_memory_region(&pmm->system_accessible_frames, addr, len);
    }

    //map out kernel image region
    unset_memory_region(&pmm->system_accessible_frames, info->kernel_image_start, frame_ceil(info->kernel_image_size));
    //map out framebuffer
    unset_memory_region(&pmm->system_accessible_frames, info->framebuffer.address, frame_ceil(info->framebuffer.size));
}

static uint32_t index_of_first_set_bit_in_bitmap(address_space_frame_bitmap_t* bitmap) {
    return index_of_first_bit_with_value_in_bitmap(bitmap, true);
}

static uint32_t index_of_first_unset_bit_in_bitmap(address_space_frame_bitmap_t* bitmap) {
    return index_of_first_bit_with_value_in_bitmap(bitmap, false);
}

uint32_t pmm_alloc(void) {
    pmm_state_t* pmm = pmm_get();
    uint32_t index = first_usable_pmm_index(pmm);
    uint32_t frame_address = index * PAGING_FRAME_SIZE;
    bitmap_set_address(&pmm->allocation_state, frame_address);
    return frame_address;
}

void pmm_free(uint32_t frame_address) {
    pmm_state_t* pmm = pmm_get();
    //sanity check
    if (!bitmap_check_address(&pmm->allocation_state, frame_address)) {
        panic("attempted to free non-allocated frame");
    }
    bitmap_unset_address(&pmm->allocation_state, frame_address);
}