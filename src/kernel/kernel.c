//freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>

//kernel headers
#include <kernel/drivers/vga_screen/vga_screen.h>
#include <kernel/multiboot.h>
#include <kernel/boot.h>

#define NotImplemented() do {_assert("Not implemented", __FILE__, __LINE__);} while(0);
#define assert(msg) do {_assert(msg, __FILE__, __LINE__);} while(0);

void _assert(const char* msg, const char* file, int line) {
	printf("Kernel assertion. %s, line %d: %s\n", file, line, msg);
	asm("cli");
	while (1) {}
}

typedef enum physical_memory_region_type {
	REGION_USABLE,
	REGION_RESERVED
} physical_memory_region_type;

typedef struct physical_memory_region {
	physical_memory_region_type type;
	uint32_t addr;
	uint32_t len;
} physical_memory_region_t;

typedef struct boot_info {
	uint32_t boot_stack_top_phys;
	uint32_t boot_stack_bottom_phys;
	uint32_t boot_stack_size;

	uint32_t mem_region_count;
	physical_memory_region_t mem_regions[32];
} boot_info_t;

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

static void multiboot_interpret_boot_device(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_BOOTDEV)) {
		printf("No boot device set\n");
		return;
	}
	printf("Boot device set\n");
}

static void multiboot_interpret_modules(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_MODS)) {
		printf("No boot modules loaded\n");
		return;
	}
	printf("There are boot modules!\n");
}

static void multiboot_interpret_symbol_table(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (mboot_data->flags & MULTIBOOT_INFO_AOUT_SYMS) {
		printf("AOUT Symbol table available\n");
		return;
	}
	else if (mboot_data->flags & MULTIBOOT_INFO_ELF_SHDR) {
		printf("ELF Symbol table available\n");
	}
}

static void multiboot_interpret_bootloader(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)) {
		printf("No bootloader name included\n");
		return;
	}
	printf("Bootloader name included\n");
}

static void multiboot_interpret_video_info(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_VBE_INFO)) {
		printf("No VBE info included\n");
		return;
	}
	//interpret VBE info
	printf("VBE info included\n");
	if (!(mboot_data->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
		printf("No framebuffer info included\n");
		return;
	}
	//interpret framebuffer info
	printf("Framebuffer info included\n");
}

static void multiboot_interpret(struct multiboot_info* mboot_data, boot_info_t* out_info) {
	printf("Multiboot info at 0x%x\n", mboot_data);
	printf("Multiboot flags: 0x%x\n", mboot_data->flags);
	multiboot_interpret_memory_map(mboot_data, out_info);
	multiboot_interpret_boot_device(mboot_data, out_info);
	multiboot_interpret_modules(mboot_data, out_info);
	multiboot_interpret_symbol_table(mboot_data, out_info);
	multiboot_interpret_bootloader(mboot_data, out_info);
	multiboot_interpret_video_info(mboot_data, out_info);
}

static void boot_info_read(struct multiboot_info* mboot_data, boot_info_t* boot_info) {
	boot_info->boot_stack_top_phys = &kernel_stack;
	boot_info->boot_stack_bottom_phys = &kernel_stack_bottom;
	boot_info->boot_stack_size = &kernel_stack - &kernel_stack_bottom;
	multiboot_interpret(mboot_data, boot_info);
}

static void boot_info_dump(boot_info_t* info) {
	printf("Kernel stack at 0x%x. Size: 0x%x\n", info->boot_stack_top_phys, info->boot_stack_size);

	//dump memory map
	printf("Boot-time RAM map:\n");
	for (int i = 0; i < info->mem_region_count; i++) {
		physical_memory_region_t region = info->mem_regions[i];
		char* type = "Usable  ";
		if (region.type == REGION_RESERVED) {
			type = "Reserved";
		}
		printf("\t%s RAM region 0x%x, 0x%x bytes\n", type, region.addr, region.len);
	}
}

void kernel_main(struct multiboot_info* mboot_data) {
	vga_screen_init();

	boot_info_t boot_info = {0};
	boot_info_read(mboot_data, &boot_info);
	boot_info_dump(&boot_info);
}
