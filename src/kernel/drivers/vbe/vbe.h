#ifndef VBE_H
#define VBE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VBE_DISPI_IOPORT_INDEX			0x01CE
#define VBE_DISPI_IOPORT_DATA			0x01CF
#define VBE_DISPI_INDEX_ID				0
#define VBE_DISPI_INDEX_XRES			1
#define VBE_DISPI_INDEX_YRES			2
#define VBE_DISPI_INDEX_BPP				3
#define VBE_DISPI_INDEX_BANK			5
#define VBE_DISPI_INDEX_VIRT_WIDTH		6
#define VBE_DISPI_INDEX_VIRT_HEIGHT		7
#define VBE_DISPI_INDEX_X_OFFSET		8
#define VBE_DISPI_INDEX_Y_OFFSET		9

#define VBE_DISPI_BPP_4					0x04
#define VBE_DISPI_BPP_8					0x08
#define VBE_DISPI_BPP_15				0x0F
#define VBE_DISPI_BPP_16				0x10
#define VBE_DISPI_BPP_24				0x18
#define VBE_DISPI_BPP_32				0x20

#define VBE_DISPI_DISABLED				0x00 
#define VBE_DISPI_ENABLED				0x01
#define VBE_DISPI_INDEX_ENABLE			0x04
#define VBE_DISPI_LFB_ENABLED			0x40
#define VBE_DISPI_NOCLEARMEM			0x80

#define VBE_DISPI_ID0					0xB0C0
#define VBE_DISPI_ID1					0xB0C1
#define VBE_DISPI_ID2					0xB0C2
#define VBE_DISPI_ID3					0xB0C3
#define VBE_DISPI_ID4					0xB0C4
#define VBE_DISPI_ID5					0xB0C5

#define VBE_DISPI_LFB_PHYSICAL_ADDRESS	0xA0000
#define BANK_SIZE						0x10000

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

//read/write to VBE registers
void vbe_write_reg(unsigned short idx, unsigned short val);
unsigned short vbe_read_reg(unsigned short idx);

//is VBE available in our environment?
bool vbe_available(void);

//switch video memory bank to 'bank_num'
void vbe_set_bank(unsigned short bank_num);

//perform VBE graphics mode switch using given parameters
//if 'use_lfb' is set pixels can be plotted directly onto a framebuffer
//if 'clear_vmem' is set we'll request VBE clear framebuffer before we recieve it
void vbe_set_video_mode(unsigned int width, unsigned int height, unsigned int depth, bool use_lfb, bool clear_vmem);

#endif

