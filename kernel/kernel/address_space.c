#include "address_space.h"

uintptr_t addr_space_frame_floor(uintptr_t addr) {
    uint32_t orig=addr;
    if (addr & ~PAGING_FRAME_MASK) {
        addr &= PAGING_FRAME_MASK;
        return addr - PAGING_FRAME_SIZE;
    }
    return addr;

}

uintptr_t addr_space_page_floor(uintptr_t addr) {
    return addr_space_frame_floor(addr);
}

uintptr_t addr_space_frame_ceil(uintptr_t addr) {
    if (addr & ~PAGING_FRAME_MASK) {
        addr &= PAGING_FRAME_MASK;
        return addr + PAGING_FRAME_SIZE;
    }
    return addr;
}

uintptr_t addr_space_page_ceil(uintptr_t addr) {
    return addr_space_frame_ceil(addr);
}
