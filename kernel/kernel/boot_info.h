#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/vmm/vmm.h>
#include <kernel/elf.h>
#include <kernel/smp.h>
#include <bootloader/axle_boot_info.h>

typedef enum physical_memory_region_type {
    PHYS_MEM_REGION_USABLE,
    PHYS_MEM_REGION_RESERVED,
    PHYS_MEM_REGION_RESERVED_ACPI_NVM,
    PHYS_MEM_REGION_RESERVED_AXLE_KERNEL_CODE_AND_DATA,
    PHYS_MEM_REGION_RESERVED_ACPI_TABLES,
} physical_memory_region_type;

typedef struct physical_memory_region {
    physical_memory_region_type type;
    uintptr_t addr;
    size_t len;
} physical_memory_region_t;

typedef struct framebuffer_info {
    uintptr_t address;
    uintptr_t width;
    uintptr_t height;
    uint8_t bits_per_pixel;
    uint8_t bytes_per_pixel;
    uint32_t pixels_per_scanline;
    uintptr_t size;
} framebuffer_info_t;

typedef struct boot_info {
    uintptr_t kernel_image_start;
    uintptr_t kernel_image_end;
    uintptr_t kernel_image_size;

    /*
    uint32_t boot_stack_top_phys;
    uint32_t boot_stack_bottom_phys;
    uint32_t boot_stack_size;
    */

    uintptr_t file_server_elf_start;
    uintptr_t file_server_elf_end;
    uintptr_t file_server_elf_size;

    uintptr_t initrd_start;
    uintptr_t initrd_end;
    uintptr_t initrd_size;

    uint32_t mem_region_count;
    physical_memory_region_t mem_regions[256];

    //multiboot_boot_device_t boot_device;
    //multiboot_elf_section_header_table_t symbol_table_info;
    elf_t kernel_elf_symbol_table;
    framebuffer_info_t framebuffer;

    vas_state_t* vas_kernel;

    uint32_t ms_per_pit_tick;

    uintptr_t acpi_rsdp;

    uintptr_t ap_bootstrap_base;
    uintptr_t ap_bootstrap_size;

    smp_info_t* smp_info;
} boot_info_t;

boot_info_t* boot_info_get(void);
void boot_info_read(axle_boot_info_t* boot_info);
void boot_info_dump();

#endif
