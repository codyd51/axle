#include <std/common.h>

#define MULTIBOOT_FLAG_MEM	0x001
#define MULTIBOOT_FLAG_DEVICE	0x002
#define MULTIBOOT_FLAGS_CMDLINE	0x004
#define MULTIBOOT_FLAG_MODS	0x008
#define MULTIBOOT_FLAG_AOUT	0x010
#define MULTIBOOT_FLAG_ELF	0x020
#define MULTIBOOT_FLAG_MMAP	0x040
#define MULTIBOOT_FLAG_CONFIG	0x080
#define MULTIBOOT_FLAG_LOADER	0x100
#define MULTIBOOT_FLAG_APM	0x200
#define MULTIBOOT_FLAG_VBE	0x400

typedef struct multiboot_info {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t num;
	uint32_t size;
	uint32_t addr;
	uint32_t shndx;
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint32_t vbe_mode;
	uint32_t vbe_interface_seg;
	uint32_t vbe_interface_off;
	uint32_t vbe_interface_len;
} multiboot; 

//typedef struct multiboot_header multiboot_header_t;
