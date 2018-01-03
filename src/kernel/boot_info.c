//freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>
#include <std/memory.h>

//kernel headers
#include <kernel/drivers/vga_screen/vga_screen.h>
#include <kernel/multiboot.h>
#include <kernel/boot.h>
#include <kernel/assert.h>

#include "boot_info.h"

static void multiboot_interpret_memory_map(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_MEMORY)) {
		assert("No memory map available!");
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
	printf("Booted from disk 0x%x partition %d:%d:%d\n", 
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
	printf("There are boot modules!\n");
	NotImplemented();
}

static void multiboot_interpret_symbol_table(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (mboot_data->flags & MULTIBOOT_INFO_AOUT_SYMS) {
		//a.out symbol table available
		NotImplemented();
	}
	else if (mboot_data->flags & MULTIBOOT_INFO_ELF_SHDR) {
		out_info->symbol_table_info = mboot_data->u.elf_sec;
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

static void multiboot_interpret_video_info(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_VBE_INFO)) {
		printf("No VBE info included\n");
		return;
	}
	//VBE fields are valid
	if (!(mboot_data->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
		printf("No framebuffer info included\n");
		return;
	}
	//framebuffer fields are valid
	char* framebuffer_type;
	switch (mboot_data->framebuffer_type) {
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
	printf("%s framebuffer at 0x%08x. %d x %d, %dbpp\n", 
		framebuffer_type, 
		(uint32_t)mboot_data->framebuffer_addr,
		mboot_data->framebuffer_width,
		mboot_data->framebuffer_height,
		mboot_data->framebuffer_bpp
	);
}

static void multiboot_interpret(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	multiboot_interpret_bootloader(mboot_data, out_info);
	multiboot_interpret_memory_map(mboot_data, out_info);
	multiboot_interpret_boot_device(mboot_data, out_info);
	multiboot_interpret_modules(mboot_data, out_info);
	multiboot_interpret_symbol_table(mboot_data, out_info);
	multiboot_interpret_video_info(mboot_data, out_info);
}

boot_info_t* boot_info_get(void) {
    static boot_info_t boot_info = {0};
    return &boot_info;
}

void boot_info_read(struct multiboot_info* mboot_data) {
    boot_info_t* boot_info = boot_info_get();
    memset(boot_info, 0, sizeof(boot_info_t));

	boot_info->boot_stack_top_phys = (uint32_t)&kernel_stack;
	boot_info->boot_stack_bottom_phys = (uint32_t)&kernel_stack_bottom;
	boot_info->boot_stack_size = boot_info->boot_stack_top_phys - boot_info->boot_stack_bottom_phys;

    boot_info->kernel_image_start = (uint32_t)&kernel_image_start;
    boot_info->kernel_image_end = (uint32_t)&kernel_image_end;
    boot_info->kernel_image_size = boot_info->kernel_image_end - boot_info->kernel_image_start;

	multiboot_interpret(mboot_data, boot_info);
}

void boot_info_dump() {
    boot_info_t* info = boot_info_get();

    printf("Kernel image at [0x%08x to 0x%08x]. Size: 0x%x\n", info->kernel_image_start, info->kernel_image_end, info->kernel_image_size);
	printf("Kernel stack at [0x%08x to 0x%08x]. Size: 0x%x\n", info->boot_stack_bottom_phys, info->boot_stack_top_phys, info->boot_stack_size);

	boot_info_dump_memory_map(info);
	boot_info_dump_boot_device(info);
	boot_info_dump_symbol_table(info);
}
