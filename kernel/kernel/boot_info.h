#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/vmm/vmm.h>
#include <kernel/elf.h>
#include <bootloader/axle_boot_info.h>

typedef enum physical_memory_region_type {
    PHYS_MEM_REGION_USABLE,
    PHYS_MEM_REGION_RESERVED,
    PHYS_MEM_REGION_RESERVED_ACPI_NVM,
} physical_memory_region_type;

typedef struct physical_memory_region {
    physical_memory_region_type type;
    uintptr_t addr;
    size_t len;
} physical_memory_region_t;

typedef struct framebuffer_info {
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t bytes_per_pixel;
    uint32_t size;
} framebuffer_info_t;

typedef struct boot_info {
    uint32_t kernel_image_start;
    uint32_t kernel_image_end;
    uint32_t kernel_image_size;

    /*
    uint32_t boot_stack_top_phys;
    uint32_t boot_stack_bottom_phys;
    uint32_t boot_stack_size;
    */

    uint32_t initrd_start;
    uint32_t initrd_end;
    uint32_t initrd_size;

    uint32_t mem_region_count;
    physical_memory_region_t mem_regions[256];

    //multiboot_boot_device_t boot_device;
    //multiboot_elf_section_header_table_t symbol_table_info;
    elf_t kernel_elf_symbol_table;
    framebuffer_info_t framebuffer;

    vmm_page_directory_t* vmm_kernel;

    uint32_t ms_per_pit_tick;
} boot_info_t;

boot_info_t* boot_info_get(void);
void boot_info_read(axle_boot_info_t* boot_info);
void boot_info_dump();

#endif
