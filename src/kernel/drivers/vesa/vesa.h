#include <std/common.h>

/*
struct vbe_info_block {
	char vbe_signature[4]; //"VESA"
	uint16_t vbe_version; //0x300 for VBE 3.0
	uint16_t oem_str_ptr[2]; //isa vbe_far_ptr
	uint8_t capabilities[4];
	uint16_t video_mode_ptr[2]; //isa vbe_far_ptr
	uint16_t total_memory; //as # of 64kb blocks
} __attribute__((packed));


typedef struct vesa_info {
	unsigned char vesa_signature[4] 	__attribute__((packed)); //"VESA"
	unsigned short vesa_version		__attribute__((packed)); //0x300 for 0x300 for vbe 3.0
	unsigned long oem_str_ptr		__attribute__((packed)); 
	unsigned char capabilities[4]		__attribute__((packed));
	unsigned long video_mode_ptr		__attribute__((packed));
	unsigned short total_memory		__attribute__((packed)); //as # of 64kb blocks
	unsigned short oem_software_rev		__attribute__((packed));
	unsigned long oem_vendor_name_ptr	__attribute__((packed));
	unsigned long oem_product_name_ptr	__attribute__((packed));
	unsigned long oem_product_rev_ptr	__attribute__((packed));
	unsigned char reserved[222]		__attribute__((packed));
	unsigned char oem_data[256]		__attribute__((packed));
} vesa_info;

typedef struct mode_info {
	unsigned short mode_attributes		__attribute__((packed));
	unsigned char wina_attributes		__attribute__((packed));
	unsigned char winb_attributes		__attribute__((packed));
	unsigned char win_granularity		__attribute__((packed));
	unsigned short win_size			__attribute__((packed));
	unsigned short wina_segment		__attribute__((packed));
	unsigned short winb_segment		__attribute__((packed));
	unsigned long win_func_ptr		__attribute__((packed));
	unsigned short bytes_per_scan_line	__attribute__((packed));
	unsigned short x_resolution		__attribute__((packed));
	unsigned short y_resoltion		__attribute__((packed));
	unsigned char x_char_size		__attribute__((packed));
	unsigned char y_char_size		__attribute__((packed));
	unsigned char number_planes		__attribute__((packed));
	unsigned char bits_per_pixel		__attribute__((packed));
	unsigned char number_banks		__attribute__((packed));
	unsigned char memory_model		__attribute__((packed));
	unsigned char bank_size			__attribute__((packed));
	unsigned char number_image_pages	__attribute__((packed));
	unsigned char reserved_page		__attribute__((packed));
	unsigned char red_mask_size		__attribute__((packed));
	unsigned char red_mask_pos		__attribute__((packed));
	unsigned char green_mask_size		__attribute__((packed));
	unsigned char green_mask_pos		__attribute__((packed));
	unsigned char blue_mask_size		__attribute__((packed));
	unsigned char blue_mask_pos		__attribute__((packed));
	unsigned char reserved_mask_size	__attribute__((packed));
	unsigned char reserved_mask_pos		__attribute__((packed));
	unsigned char direct_color_mode_info 	__attribute__((packed));
	unsigned long phys_base_ptr		__attribute__((packed));
	unsigned long off_screen_mem_offset	__attribute__((packed));
	unsigned short off_screen_mem_size	__attribute__((packed));
	unsigned char reserved[206]		__attribute__((packed));
} mode_info;
*/

typedef struct vbe_info_block {
	char vbe_signature[4]; //"VESA"
	uint16_t vbe_version; //0x300 for VBE 3.0
	uint16_t oem_string_ptr[2]; 
	uint8_t capabilities[4];
	uint16_t video_mode_ptr[2];
	uint16_t total_memory; //as # of 64kb blocks
} vbe_info_block;

typedef struct vesa_mode_info {
	uint16_t attributes;
	uint8_t win_a;
	uint8_t win_b;
	uint16_t granularity;
	uint16_t win_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;
	uint16_t pitch; //bytes per scanline

	uint16_t x_res;
	uint16_t y_res;
	uint8_t w_char, y_char, planes, bpp, banks;
	uint8_t memory_model, bank_size, image_pages;
	uint8_t reserved;

	uint8_t red_mask, red_position;
	uint8_t green_mask, green_position;
	uint8_t blue_mask, blue_position;
	uint8_t rsv_mask, rsv_position;
	uint8_t directcolor_attributes;

	uint32_t physbase; //linear framebuffer address of screen
	uint32_t reserved1;
	uint16_t reserved2;
} vesa_mode_info;

vesa_mode_info* get_vesa_info();
void vesa_test();
