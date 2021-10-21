#ifndef AXLE_BOOT_INFO_H
#define AXLE_BOOT_INFO_H

typedef uint64_t axle_phys_addr_t;
typedef uint64_t axle_virt_addr_t;

typedef struct {
    uint32_t                Type;
    uint32_t                Pad;
    axle_phys_addr_t  PhysicalStart;
    axle_virt_addr_t   VirtualStart;
    uint64_t                NumberOfPages;
    uint64_t                Attribute;
} axle_efi_memory_descriptor_t;


typedef struct axle_boot_info {
	// Graphics info
	axle_phys_addr_t framebuffer_base;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bytes_per_pixel;

	// Memory map info
	uint64_t memory_map_size;
	uint64_t memory_descriptor_size;
	axle_efi_memory_descriptor_t* memory_descriptors;

	// initrd info
	uint64_t initrd_base;
	uint64_t initrd_size;
} axle_boot_info_t;

#endif
