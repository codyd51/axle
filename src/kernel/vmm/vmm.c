#include "vmm.h"

#include <kernel/pmm/pmm.h>

void vmm_init(void) {
    //instantiate kernel page directory
    uint32_t frames_in_directory = sizeof(vmm_page_directory_t) / PAGING_FRAME_SIZE;
    pmm_alloc();
}