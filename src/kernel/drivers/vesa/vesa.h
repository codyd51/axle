#ifndef VESA_H
#define VESA_H

#include "gfx/lib/gfx.h"

//typedef for VESA mode information
typedef struct vbe_mode_info {
		unsigned short mode_attributes		__attribute__((packed));
		unsigned char win_a_attributes;
		unsigned char win_b_attributes;
		unsigned short win_granularity		__attribute__((packed));
		unsigned short win_size				__attribute__((packed));
		unsigned short win_a_segment		__attribute__((packed));
		unsigned short win_b_segment		__attribute__((packed));
		unsigned long win_func_ptr			__attribute__((packed));
		unsigned short bytes_per_scan_line	__attribute__((packed));
		unsigned short x_res				__attribute__((packed));
		unsigned short y_res				__attribute__((packed));
		unsigned char x_char_size;
		unsigned char y_char_size;
		unsigned char num_planes;
		unsigned char bpp;
		unsigned char bank_num;
		unsigned char mem_model;
		unsigned char bank_size;
		unsigned char num_image_pages;
		unsigned char reserved_page;
		unsigned char red_mask_size;
		unsigned char red_mask_pos;
		unsigned char green_mask_size;
		unsigned char green_mask_pos;
		unsigned char blue_mask_size;
		unsigned char blue_mask_pos;
		unsigned char reserved_mask_size;
		unsigned char reserved_mask_pos;
		unsigned char direct_color_mode_info;
		unsigned long physbase				__attribute__((packed));
		unsigned long offscreen_mem_offset	__attribute__((packed));
		unsigned short offscreen_mem_size	__attribute__((packed));
		unsigned char reserved[256];
} vbe_mode_info;

typedef struct vesa_info {
		unsigned char vesa_signature[4];
		unsigned short vesa_version			__attribute__((packed));
		unsigned long oem_string_ptr		__attribute__((packed));
		unsigned char capabilities[4];
		unsigned long video_mode_ptr		__attribute__((packed));
		unsigned short total_memory			__attribute__((packed));
		unsigned short oem_software_rev		__attribute__((packed));
		unsigned long oem_vendor_name_ptr	__attribute__((packed));
		unsigned long oem_product_name_ptr	__attribute__((packed));
		unsigned long oem_product_rev_ptr	__attribute__((packed));
		unsigned char reserved[222];
		unsigned char oem_data[256];
} vesa_info;

screen_t* get_vesa_screen();

#endif
