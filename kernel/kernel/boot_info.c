//freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>
#include <std/memory.h>
#include <std/common.h>

//kernel headers
#include <kernel/multiboot.h>
#include <kernel/boot.h>
#include <kernel/assert.h>
#include <kernel/elf.h>

#include "boot_info.h"

static void multiboot_interpret_memory_map(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    if (!(mboot_data->flags & MULTIBOOT_INFO_MEMORY)) {
        panic("No memory map available!");
    }
    if (!(mboot_data->flags & MULTIBOOT_INFO_MEM_MAP)) {
        //we must interpret the basic memory map
        NotImplemented();
    }

    uint32_t region_count = 0;
    /*
    uint32_t read_byte_count = 0;
    while (read_byte_count < mboot_data->mmap_length) {
        struct multiboot_mmap_entry* ent = (struct multiboot_mmap_entry*)(mboot_data->mmap_addr + read_byte_count);

        out_info->mem_regions[region_count].addr = ent->addr;
        out_info->mem_regions[region_count].len = ent->len;

        physical_memory_region_type type = REGION_RESERVED;
        if (ent->type == MULTIBOOT_MEMORY_AVAILABLE) {
            type = REGION_USABLE;
        }
        out_info->mem_regions[region_count].type = type;

        //add 4 bytes extra because the size field does not include the size of the size field itself
        //the size field is a uint32_t, so add the value of the size field + sizeof(uint32_t)
        read_byte_count += ent->size + sizeof(ent->size);
        region_count++;
    }
    */
    out_info->mem_region_count = region_count;
}

static void boot_info_dump_memory_map(boot_info_t* info) {
    printf("Boot-time RAM map:\n");
    for (int i = 0; i < info->mem_region_count; i++) {
        physical_memory_region_t region = info->mem_regions[i];
        const char* type = NULL;
        switch (region.type) {
            case PHYS_MEM_REGION_USABLE:
                type = "Usabale ";
                break;
            case PHYS_MEM_REGION_RESERVED:
                type = "Reserved";
                break;
            case PHYS_MEM_REGION_RESERVED_ACPI_NVM:
                type = "ACPI NVM";
                break;
            case PHYS_MEM_REGION_RESERVED_AXLE_KERNEL_CODE_AND_DATA:
                type = "Kernel  ";
                break;
            default:
                type = "Unknown ";
                break;
        }
        printf("\t%s RAM region 0x%08x, 0x%08x bytes\n", type, region.addr, region.len);
    }
}

static void multiboot_interpret_boot_device(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    if (!(mboot_data->flags & MULTIBOOT_INFO_BOOTDEV)) {
        printf("No boot device set\n");
        return;
    }
    uint32_t boot_device_val = mboot_data->boot_device;
}

static void multiboot_interpret_modules(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    if (!(mboot_data->flags & MULTIBOOT_INFO_MODS)) {
        printf("0 boot modules.\n");
        return;
    }

    uint32_t mods_count = mboot_data->mods_count;
    printf("%d boot modules detected\n", mods_count);
    assert(mods_count >= 1, "Modules flag was set, but no modules reported");

    multiboot_module_t* mod = (multiboot_module_t*)mboot_data->mods_addr;
    for (int i = 0; i < mods_count; i++) {

        if (!strcmp(mod->cmdline, "initrd.img")) {
            printf("Found initrd");
            printf_info("Initrd @ 0x%08x - 0x%08x", mod->mod_start, mod->mod_end);
            out_info->initrd_start = mod->mod_start;
            out_info->initrd_end = mod->mod_end;
            out_info->initrd_size = mod->mod_end - mod->mod_start;
        }
        else {
            printf("Unknown boot module: %s", mod->cmdline);
            panic("Unknown boot module");
        }

        mod = (multiboot_module_t*)mod->mod_end;
    }
}

static void multiboot_interpret_symbol_table(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    if (mboot_data->flags & MULTIBOOT_INFO_AOUT_SYMS) {
        //a.out symbol table available
        NotImplemented();
    }
    else if (mboot_data->flags & MULTIBOOT_INFO_ELF_SHDR) {
        //out_info->symbol_table_info = mboot_data->u.elf_sec;
        //elf_from_multiboot(mboot_data, &out_info->kernel_elf_symbol_table);
    }
}

static void boot_info_dump_symbol_table(boot_info_t* info) {
    //printf("Symbol table: %d entries starting at 0x%08x\n", info->symbol_table_info.num, info->symbol_table_info.addr);
}

static void multiboot_interpret_framebuffer(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    if (!(mboot_data->flags & MULTIBOOT_INFO_VBE_INFO)) {
        panic("no VBE info\n");
        return;
    }
    if (!(mboot_data->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        panic("no framebuffer info\n");
        return;
    }
    out_info->framebuffer.address = mboot_data->framebuffer_addr;
    out_info->framebuffer.width = mboot_data->framebuffer_width;
    out_info->framebuffer.height = mboot_data->framebuffer_height;
    out_info->framebuffer.bits_per_pixel = mboot_data->framebuffer_bpp;
    out_info->framebuffer.bytes_per_pixel = (int)(out_info->framebuffer.bits_per_pixel / BITS_PER_BYTE);
    out_info->framebuffer.size = out_info->framebuffer.width * out_info->framebuffer.height * out_info->framebuffer.bytes_per_pixel;
    printf("out_info framebuffer 0x%08x\n", out_info->framebuffer.address);
}

static void boot_info_dump_framebuffer(boot_info_t* info) {
    framebuffer_info_t fb_info = info->framebuffer;

    printf(
        "framebuffer resolution: %d x %d @ %d bpp, %d px/scanline\n", 
        fb_info.width,
        fb_info.height,
        fb_info.bits_per_pixel,
        fb_info.pixels_per_scanline
    );
    printf("Framebuffer  at [0x%p to 0x%p]. Size: 0x%p\n", fb_info.address, fb_info.address+fb_info.size, fb_info.size);
}

static void multiboot_interpret(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    multiboot_interpret_memory_map(mboot_data, out_info);
    multiboot_interpret_boot_device(mboot_data, out_info);
    multiboot_interpret_modules(mboot_data, out_info);
    multiboot_interpret_symbol_table(mboot_data, out_info);
    multiboot_interpret_framebuffer(mboot_data, out_info);
}

boot_info_t* boot_info_get(void) {
    static boot_info_t boot_info = {0};
    return &boot_info;
}

/*
void boot_info_read(struct multiboot_info* mboot_data) {
    boot_info_t* boot_info = boot_info_get();
    memset(boot_info, 0, sizeof(boot_info_t));

    boot_info->boot_stack_top_phys = (uint32_t)&_kernel_stack_top;
    boot_info->boot_stack_bottom_phys = (uint32_t)&_kernel_stack_bottom;
    boot_info->boot_stack_size = boot_info->boot_stack_top_phys - boot_info->boot_stack_bottom_phys;

    boot_info->kernel_image_start = (uint32_t)&_kernel_image_start;
    boot_info->kernel_image_end = (uint32_t)&_kernel_image_end;
    boot_info->kernel_image_size = boot_info->kernel_image_end - boot_info->kernel_image_start;

    multiboot_interpret(mboot_data, boot_info);
}
*/

static const char* _name_for_mem_region_type(axle_efi_memory_type_t type) {
    switch (type) {
        case EFI_MEMORY_RESERVED:
            return "Reserved";
        case EFI_LOADER_CODE:
            return "Loader code";
        case EFI_LOADER_DATA:
            return "Loader data";
        case EFI_BOOT_SERVICES_CODE:
            return "Boot services code";
        case EFI_BOOT_SERVICES_DATA:
            return "Boot services data";
        case EFI_RUNTIME_SERVICES_CODE:
            return "Runtime services code";
        case EFI_RUNTIME_SERVICES_DATA:
            return "Runtime services data";
        case EFI_CONVENTIONAL_MEMORY:
            return "Conventional memory";
        case EFI_UNUSABLE_MEMORY:
            return "Unusable memory";
        case EFI_ACPI_RECLAIM_MEMORY:
            return "ACPI reclaim memory";
        case EFI_ACPI_MEMORY_NVS:
            return "ACPI memory NVS";
        case EFI_MEMORY_MAPPED_IO:
            return "Memory-mapped IO";
        case EFI_MEMORY_MAPPED_IO_PORT_SPACE:
            return "Memory-mapped IO port space";
        case EFI_PAL_CODE:
            return "PAL code";
        case EFI_MEMORY_TYPE_AXLE_KERNEL_IMAGE:
            return "axle kernel";
        case EFI_MEMORY_TYPE_AXLE_INITRD:
            return "axle initrd";
        case EFI_MEMORY_TYPE_AXLE_PAGING_STRUCTURE:
            return "axle paging struct";
        case EFI_MAX_MEMORY_TYPE:
        default:
            return "Max/unknown memory type";
    }
}
void boot_info_read(axle_boot_info_t* bootloader_info) {
    boot_info_t* boot_info = boot_info_get();
    memset(boot_info, 0, sizeof(boot_info_t));

    boot_info->framebuffer.address = bootloader_info->framebuffer_base;
    boot_info->framebuffer.width = bootloader_info->framebuffer_width;
    boot_info->framebuffer.height = bootloader_info->framebuffer_height;
    boot_info->framebuffer.bytes_per_pixel = bootloader_info->framebuffer_bytes_per_pixel;
    boot_info->framebuffer.bits_per_pixel = bootloader_info->framebuffer_bytes_per_pixel * 8;
    boot_info->framebuffer.size = (bootloader_info->framebuffer_pixels_per_scanline * bootloader_info->framebuffer_height * bootloader_info->framebuffer_bytes_per_pixel);
    boot_info->framebuffer.pixels_per_scanline = bootloader_info->framebuffer_pixels_per_scanline;

    boot_info->initrd_start = bootloader_info->initrd_base;
    boot_info->initrd_size = bootloader_info->initrd_size;
    boot_info->initrd_end = bootloader_info->initrd_base + bootloader_info->initrd_size;

    boot_info->kernel_elf_symbol_table.strtab = PMA_TO_VMA(bootloader_info->kernel_string_table_base);
    boot_info->kernel_elf_symbol_table.strtabsz = bootloader_info->kernel_string_table_size;
    boot_info->kernel_elf_symbol_table.symtab = PMA_TO_VMA(bootloader_info->kernel_symbol_table_base);
    boot_info->kernel_elf_symbol_table.symtabsz = bootloader_info->kernel_symbol_table_size;

    boot_info->mem_region_count = bootloader_info->memory_map_size / bootloader_info->memory_descriptor_size;
    for (uint32_t i = 0; i < boot_info->mem_region_count; i++) {
        axle_efi_memory_descriptor_t* mem_desc = (axle_efi_memory_descriptor_t*)((uint8_t*)(bootloader_info->memory_descriptors) + (i * bootloader_info->memory_descriptor_size));
        //printf("%d phys 0x%x (%d pages), flags: 0x%x, type: %s\n", i, mem_desc->phys_start, mem_desc->page_count, mem_desc->flags, _name_for_mem_region_type(mem_desc->type));
        physical_memory_region_t* region = &boot_info->mem_regions[i];
        region->addr = mem_desc->phys_start;
        region->len = mem_desc->page_count * PAGE_SIZE;

        if (mem_desc->type == EFI_BOOT_SERVICES_CODE ||
            mem_desc->type == EFI_BOOT_SERVICES_DATA || 
            mem_desc->type == EFI_LOADER_CODE || 
            mem_desc->type == EFI_LOADER_DATA || 
            mem_desc->type == EFI_CONVENTIONAL_MEMORY) {
            region->type = PHYS_MEM_REGION_USABLE;
        }
        else if (mem_desc->type == EFI_RUNTIME_SERVICES_CODE || 
                 mem_desc->type == EFI_RUNTIME_SERVICES_DATA || 
                 mem_desc->type == EFI_MEMORY_MAPPED_IO || 
                 mem_desc->type == EFI_MEMORY_RESERVED) {
            region->type = PHYS_MEM_REGION_RESERVED;
        }
        else if (mem_desc->type == EFI_ACPI_MEMORY_NVS ||
                 mem_desc->type == EFI_ACPI_RECLAIM_MEMORY) {
            region->type = PHYS_MEM_REGION_RESERVED_ACPI_NVM;
        }
        else if (mem_desc->type == EFI_PAL_CODE) {
            // Ref: https://forum.osdev.org/viewtopic.php?f=1&t=55980&sid=c44c69b84bdf42a2dc92a878ca00abbc
            region->type = PHYS_MEM_REGION_RESERVED_AXLE_KERNEL_CODE_AND_DATA;
        }
        else {
            printf("Unrecognized memory region type %p\n", mem_desc->type);
            //assert(false, "Unrecognized memory region type");
            region->type = PHYS_MEM_REGION_RESERVED;
        }
    }
}

void boot_info_dump() {
    boot_info_t* info = boot_info_get();

    boot_info_dump_framebuffer(info);
    printf("Kernel image at [0x%08x to 0x%08x]. Size: 0x%x\n", info->kernel_image_start, info->kernel_image_end, info->kernel_image_size);
    printf("inirtd image at [0x%08x to 0x%08x]. Size: 0x%x\n", info->initrd_start, info->initrd_end, info->initrd_size);
    //printf("Kernel stack at [0x%08x to 0x%08x]. Size: 0x%x\n", info->boot_stack_bottom_phys, info->boot_stack_top_phys, info->boot_stack_size);

    printf("Kernel string table: [0x%p - 0x%p]\n", info->kernel_elf_symbol_table.strtab, info->kernel_elf_symbol_table.strtab + info->kernel_elf_symbol_table.strtabsz);
    printf("Kernel symbol table: [0x%p - 0x%p]\n", info->kernel_elf_symbol_table.symtab, info->kernel_elf_symbol_table.symtab + info->kernel_elf_symbol_table.symtabsz);

    //boot_info_dump_memory_map(info);
    //boot_info_dump_symbol_table(info);
}
