#include "common.h"

/*
struct vbe_info_block {
	char vbe_signature[4]; //"VESA"
	u16int vbe_version; //0x300 for VBE 3.0
	u16int oem_str_ptr[2]; //isa vbe_far_ptr
	u8int capabilities[4];
	u16int video_mode_ptr[2]; //isa vbe_far_ptr
	u16int total_memory; //as # of 64kb blocks
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

struct vbe_info_block {
	char vbe_signature[4]; //"VESA"
	u16int vbe_version; //0x300 for VBE 3.0
	u16int oem_string_ptr[2]; 
	u8int capabilities[4];
	u16int video_mode_ptr[2];
	u16int total_memory; //as # of 64kb blocks
} __attribute__((packed));

typedef struct vesa_mode_info {
	u16int attributes;
	u8int win_a;
	u8int win_b;
	u16int granularity;
	u16int win_size;
	u16int segment_a;
	u16int segment_b;
	unsigned long win_func_ptr;
	u16int pitch; //bytes per scanline

	u16int x_res;
	u16int y_res;
	u8int w_char, y_char, planes, bpp, banks;
	u8int memory_model, bank_size, image_pages;
	u8int reserved;

	u8int red_mask, red_position;
	u8int green_mask, green_position;
	u8int blue_mask, blue_position;
	u8int rsv_mask, rsv_position;
	u8int directcolor_attributes;

	u32int physbase; //linear framebuffer address of screen
	u32int reserved1;
	u16int reserved2;
} vesa_mode_info __attribute__((packed));

vesa_mode_info* get_vesa_info();
void vesa_test();
