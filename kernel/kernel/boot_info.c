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
#include <kernel/drivers/text_mode/text_mode.h>
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

    uint32_t read_byte_count = 0;
    uint32_t region_count = 0;
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
    out_info->mem_region_count = region_count;
}

static void boot_info_dump_memory_map(boot_info_t* info) {
    printf("Boot-time RAM map:\n");
    for (int i = 0; i < info->mem_region_count; i++) {
        physical_memory_region_t region = info->mem_regions[i];
        char* type = "Usable  ";
        if (region.type == REGION_RESERVED) {
            type = "Reserved";
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
    out_info->boot_device.drive      = boot_device_val & 0xFF000000;
    out_info->boot_device.partition1 = boot_device_val & 0x00FF0000;
    out_info->boot_device.partition2 = boot_device_val & 0x0000FF00;
    out_info->boot_device.partition3 = boot_device_val & 0x000000FF;
}

static void boot_info_dump_boot_device(boot_info_t* info) {
    printf("Booted from disk 0x%02x partition %d:%d:%d\n", 
        info->boot_device.drive,
        info->boot_device.partition1,
        info->boot_device.partition2,
        info->boot_device.partition3
    );
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
        out_info->symbol_table_info = mboot_data->u.elf_sec;
        elf_from_multiboot(mboot_data, &out_info->kernel_elf_symbol_table);
    }
}

static void boot_info_dump_symbol_table(boot_info_t* info) {
    printf("Symbol table: %d entries starting at 0x%08x\n", info->symbol_table_info.num, info->symbol_table_info.addr);
}

static void multiboot_interpret_bootloader(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    if (!(mboot_data->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)) {
        printf("No bootloader name included\n");
        return;
    }
    const char* bootloader_name = (const char*)mboot_data->boot_loader_name;
    printf("Bootloader: %s\n", bootloader_name);
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
    out_info->framebuffer.type = mboot_data->framebuffer_type;
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

    char* framebuffer_type;
    switch (fb_info.type) {
        case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
            framebuffer_type = "Indexed";
            break;
        case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
            framebuffer_type = "RGB";
            break;
        case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
            framebuffer_type = "Text-mode";
            break;
        default:
            framebuffer_type = "Unknown";
            break;
    }
    printf("%s framebuffer resolution: %d x %d @ %d bpp\n", 
        framebuffer_type,
        fb_info.width,
        fb_info.height,
        fb_info.bits_per_pixel);
    printf("Framebuffer  at [0x%08x to 0x%08x]. Size: 0x%x\n", fb_info.address, fb_info.address+fb_info.size, fb_info.size);
}

static void multiboot_interpret(struct multiboot_info* mboot_data, boot_info_t* out_info) {
    multiboot_interpret_bootloader(mboot_data, out_info);
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

void boot_info_dump() {
    boot_info_t* info = boot_info_get();

    boot_info_dump_framebuffer(info);
    printf("Kernel image at [0x%08x to 0x%08x]. Size: 0x%x\n", info->kernel_image_start, info->kernel_image_end, info->kernel_image_size);
    printf("Kernel stack at [0x%08x to 0x%08x]. Size: 0x%x\n", info->boot_stack_bottom_phys, info->boot_stack_top_phys, info->boot_stack_size);

    boot_info_dump_memory_map(info);
    boot_info_dump_boot_device(info);
    boot_info_dump_symbol_table(info);
}
