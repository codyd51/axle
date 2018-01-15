#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>

#include <kernel/multiboot.h>

typedef struct multiboot_boot_device {
    char drive;
    char partition1;
    char partition2;
    char partition3;
} multiboot_boot_device_t;

typedef enum physical_memory_region_type {
    REGION_USABLE,
    REGION_RESERVED
} physical_memory_region_type;

typedef struct physical_memory_region {
    physical_memory_region_type type;
    uint32_t addr;
    uint32_t len;
} physical_memory_region_t;

typedef struct framebuffer_info {
    uint32_t type;
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t size;
} framebuffer_info_t;

typedef struct boot_info {
    uint32_t kernel_image_start;
    uint32_t kernel_image_end;
    uint32_t kernel_image_size;

    uint32_t boot_stack_top_phys;
    uint32_t boot_stack_bottom_phys;
    uint32_t boot_stack_size;

    uint32_t mem_region_count;
    physical_memory_region_t mem_regions[32];

    multiboot_boot_device_t boot_device;
    multiboot_elf_section_header_table_t symbol_table_info;
    framebuffer_info_t framebuffer;
} boot_info_t;

boot_info_t* boot_info_get(void);
void boot_info_read(struct multiboot_info* mboot_data);
void boot_info_dump();

#endif
