#include "address_space.h"

uint32_t addr_space_frame_floor(uint32_t addr) {
    uint32_t orig=addr;
    if (addr & ~PAGING_FRAME_MASK) {
        addr &= PAGING_FRAME_MASK;
        return addr - PAGING_FRAME_SIZE;
    }
    return addr;

}

uint32_t addr_space_page_floor(uint32_t addr) {
    return addr_space_frame_floor(addr);
}

uint32_t addr_space_frame_ceil(uint32_t addr) {
    if (addr & ~PAGING_FRAME_MASK) {
        addr &= PAGING_FRAME_MASK;
        return addr + PAGING_FRAME_SIZE;
    }
    return addr;
}

uint32_t addr_space_page_ceil(uint32_t addr) {
    return addr_space_frame_ceil(addr);
}
