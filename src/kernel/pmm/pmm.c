#include <stdbool.h>
#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/address_space_bitmap.h>
#include <kernel/util/mutex/mutex.h>

#include <std/common.h>

#include "pmm.h"

void pmm_reserve_mem_region(pmm_state_t* pmm, uint32_t start, uint32_t size);

static uint32_t _find_free_region_unlocked(pmm_state_t* pmm, uint32_t region_size) {
	uint32_t run_start_idx, run_end_idx;
	bool in_run = false;
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t system_frames_entry = pmm->system_accessible_frames.set[i];
        uint32_t pmm_frames_entry = pmm->allocation_state.set[i];

        //test each bit in both bitmaps
        //if we find a bit on in both bitmaps, we've found a usable index
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            //is this frame reserved by the system? (bit is off)
            //is this frame already allocated by PMM? (bit is on)
            uint32_t frame_addr = (i * PAGING_FRAME_SIZE * BITS_PER_BITMAP_ENTRY) + (j * PAGING_FRAME_SIZE);

            if (!(system_frames_entry & (1 << j)) || pmm_frames_entry & (1 << j)) {
                if (in_run) {
                    // Run broken by inaccessible frame
                    in_run = false;
                    int idx = BITMAP_BIT_INDEX(i, j);
                }
                continue;
            }

            if (!in_run) {
                in_run = true;
                run_start_idx = BITMAP_BIT_INDEX(i, j);
            }
            else {
                run_end_idx = BITMAP_BIT_INDEX(i, j);
                uint32_t run_size = run_end_idx - run_start_idx;
                if (run_size >= (region_size / PAGING_FRAME_SIZE)) {
                    return run_start_idx;
                }
            }
        }
    }
    panic("find_free_region() found nothing!");
    return 0;
}

static uint32_t _first_usable_pmm_index_unlocked(pmm_state_t* pmm) {
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
            //we found a bit which was on in the list of accessible frames,
            //and off in the list of allocated frames
            return BITMAP_BIT_INDEX(i, j);
        }
    }
    panic("first_usable_pmm_index() found nothing!");
    return 0;
}

pmm_state_t* pmm_get(void) {
    static pmm_state_t state = {0};
    return &state;
}

void pmm_dump(void) {
    pmm_state_t* pmm = pmm_get();
    printf("Physical memory manager state:\n");
    printf("\tSystem accessible frames (ranges are allocatable):\n");
    addr_space_bitmap_dump_set_ranges(&pmm->system_accessible_frames);
    printf("\tFrame allocation state (ranges are allocated):\n");
    addr_space_bitmap_dump_set_ranges(&pmm->allocation_state);
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
        uint32_t addr = addr_space_frame_ceil(region.addr);
        uint32_t len = addr_space_frame_floor(region.len);
        addr_space_bitmap_set_range(&(pmm->system_accessible_frames), addr, len);
    }

    //for identity mapping purposes
    //reserve any allocatable memory from 0x0 to start of kernel image
    pmm_reserve_mem_region(pmm, 0x00000000, info->kernel_image_start);
    //map out kernel image region
    pmm_reserve_mem_region(pmm, info->kernel_image_start, info->kernel_image_size);
    //map out framebuffer
    pmm_reserve_mem_region(pmm, info->framebuffer.address, info->framebuffer.size);
    pmm_reserve_mem_region(pmm, info->initrd_start, info->initrd_size);

    multiboot_elf_section_header_table_t symbol_table_info = info->symbol_table_info;
	elf_section_header_t* sh = (elf_section_header_t*)symbol_table_info.addr;
	uint32_t shstrtab = sh[symbol_table_info.shndx].addr;
	for (uint32_t i = 0; i < symbol_table_info.num; i++) {
		const char* name = (const char*)(shstrtab + sh[i].name);
        printf_info("PMM reserving kernel ELF section %s", name);
        pmm_reserve_mem_region(pmm, sh[i].addr, sh[i].size);
    }
    // map out kernel symbol table and string table from ELF image
    pmm_reserve_mem_region(pmm, info->kernel_elf_symbol_table.strtab, info->kernel_elf_symbol_table.strtabsz);
    pmm_reserve_mem_region(pmm, info->kernel_elf_symbol_table.symtab, info->kernel_elf_symbol_table.symtabsz);
}

//marks a block of physical memory as unallocatable
//the size does not need to be frame-aligned, it will be aligned to the next largest frame
static void _pmm_reserve_mem_region_unlocked(pmm_state_t* pmm, uint32_t start, uint32_t size) {
    uint32_t aligned_start = addr_space_frame_floor(start);
    uint32_t aligned_size = addr_space_frame_ceil(size);
    printf("PMM reserving 0x%08x - 0x%08x\n", aligned_start, aligned_start + aligned_size);
    addr_space_bitmap_unset_range(&(pmm->system_accessible_frames), aligned_start, aligned_size);
}

static void _pmm_alloc_address_unlocked(pmm_state_t* pmm, uint32_t address) {
    //has this frame already been alloc'd?
    if (addr_space_bitmap_check_address(&pmm->allocation_state, address)) {
        pmm_dump();
        printf("\nframe 0x%08x was alloc'd twice\n", address);
        panic("PMM double alloc");
    }
    addr_space_bitmap_set_address(&pmm->allocation_state, address);
}

uint32_t _pmm_alloc_unlocked(pmm_state_t* pmm) {
    uint32_t index = _first_usable_pmm_index_unlocked(pmm);
    uint32_t frame_address = index * PAGING_FRAME_SIZE;
    _pmm_alloc_address_unlocked(pmm, frame_address);

    return frame_address;
}

static uint32_t _pmm_alloc_continuous_range_unlocked(pmm_state_t* pmm, uint32_t size) {
    if (size & PAGE_FLAG_BITS_MASK) {
        size = (size & PAGING_FRAME_MASK) + PAGING_FRAME_SIZE;
    }
    uint32_t index = _find_free_region_unlocked(pmm, size);
    uint32_t first_frame_address = index * PAGING_FRAME_SIZE;
    //printf("pmm_alloc_continuous_rang 0x%08x\n", first_frame_address);
    for (uint32_t i = 0; i < size; i += PAGING_FRAME_SIZE) {
        uint32_t frame_address = first_frame_address + i;
        _pmm_alloc_address_unlocked(pmm, frame_address);
    }

    return first_frame_address;
}

static void _pmm_free_unlocked(pmm_state_t* pmm, uint32_t frame_address) {
    //sanity check
    if (!addr_space_bitmap_check_address(&pmm->allocation_state, frame_address)) {
        panic("attempted to free non-allocated frame");
    }
    addr_space_bitmap_unset_address(&pmm->allocation_state, frame_address);
}

uint32_t find_free_region(pmm_state_t* pmm, uint32_t region_size) {
    lock(&pmm->lock);
    uint32_t ret = _find_free_region_unlocked(pmm, region_size);
    unlock(&pmm->lock);
    return ret;
}

uint32_t first_usable_pmm_index(pmm_state_t* pmm) {
    lock(&pmm->lock);
    uint32_t ret = _first_usable_pmm_index_unlocked(pmm);
    unlock(&pmm->lock);
    return ret;
}

void pmm_reserve_mem_region(pmm_state_t* pmm, uint32_t start, uint32_t size) {
    lock(&pmm->lock);
    _pmm_reserve_mem_region_unlocked(pmm, start, size);
    unlock(&pmm->lock);
}

void pmm_alloc_address(uint32_t address) {
    pmm_state_t* pmm = pmm_get();
    lock(&pmm->lock);
    _pmm_alloc_address_unlocked(pmm, address);
    unlock(&pmm->lock);
}

uint32_t pmm_alloc(void) {
    pmm_state_t* pmm = pmm_get();
    lock(&pmm->lock);
    uint32_t ret = _pmm_alloc_unlocked(pmm);
    unlock(&pmm->lock);
    return ret;
}

uint32_t pmm_alloc_continuous_range(uint32_t size) {
    pmm_state_t* pmm = pmm_get();
    lock(&pmm->lock);
    uint32_t ret = _pmm_alloc_continuous_range_unlocked(pmm, size);
    unlock(&pmm->lock);
    return ret;
}

void pmm_free(uint32_t frame_address) {
    pmm_state_t* pmm = pmm_get();
    lock(&pmm->lock);
    _pmm_free_unlocked(pmm, frame_address);
    unlock(&pmm->lock);
}