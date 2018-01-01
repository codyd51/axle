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


size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len] != '\0') {
		len++;
static void multiboot_interpret_memory_map(struct multiboot_info* mboot_data, system_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_MEMORY)) {
		assert("No memory map available!");
	}
	if (!(mboot_data->flags & MULTIBOOT_INFO_MEM_MAP)) {
		//we must interpret the basic memory map
		NotImplemented();
	}
	struct multiboot_mmap_entry ent = mboot_data->
}

static void multiboot_interpret_boot_device(struct multiboot_info* mboot_data, system_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_BOOTDEV)) {
		printf("No boot device set\n");
		return;
	}
	printf("Boot device set\n");
}

static void multiboot_interpret_modules(struct multiboot_info* mboot_data, system_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_MODS)) {
		printf("No boot modules loaded\n");
		return;
	}
	printf("There are boot modules!\n");
}

static void multiboot_interpret_symbol_table(struct multiboot_info* mboot_data, system_info_t* out_info) {
	if (mboot_data->flags & MULTIBOOT_INFO_AOUT_SYMS) {
		printf("AOUT Symbol table available\n");
		return;
	}
	else if (mboot_data->flags & MULTIBOOT_INFO_ELF_SHDR) {
		printf("ELF Symbol table available\n");
	}
}

static void multiboot_interpret_bootloader(struct multiboot_info* mboot_data, system_info_t* out_info) {
	if (!(mboot_data->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)) {
		printf("No bootloader name included\n");
		return;
	}
	printf("Bootloader name included\n");
}

static void multiboot_interpret_video_info(struct multiboot_info* mboot_data, system_info_t* out_info) {
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

static void multiboot_interpret(struct multiboot_info* mboot_data, system_info_t* out_info) {
	printf("Multiboot info at 0x%x\n", mboot_data);
	printf("Multiboot flags: 0x%x\n", mboot_data->flags);
	multiboot_interpret_memory_map(mboot_data, out_info);
	multiboot_interpret_boot_device(mboot_data, out_info);
	multiboot_interpret_modules(mboot_data, out_info);
	multiboot_interpret_symbol_table(mboot_data, out_info);
	multiboot_interpret_bootloader(mboot_data, out_info);
	multiboot_interpret_video_info(mboot_data, out_info);
}

static void system_info_read(struct multiboot_info* mboot_data, system_info_t* system) {
	system->boot_stack_top_phys = &kernel_stack;
	system->boot_stack_bottom_phys = &kernel_stack_bottom;
	system->boot_stack_size = &kernel_stack - &kernel_stack_bottom;
	multiboot_interpret(mboot_data, system);
}

static void system_info_dump(system_info_t* info) {
	printf("Kernel stack at 0x%x. Size: 0x%x\n", info->boot_stack_top_phys, info->boot_stack_size);
}

void kernel_main(struct multiboot_info* mboot_data) {
	vga_screen_init();

	

	system_info_t system;
	system_info_read(mboot_data, &system);
	system_info_dump(&system);
}
