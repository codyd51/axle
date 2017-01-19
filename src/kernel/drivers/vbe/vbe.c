#include "vbe.h"
#include <std/common.h>
#include <std/printf.h>

void vbe_write_reg(unsigned short idx, unsigned short val) {
    outw(VBE_DISPI_IOPORT_INDEX, idx);
    outw(VBE_DISPI_IOPORT_DATA, val);
}
 
unsigned short vbe_read_reg(unsigned short idx) {
    outw(VBE_DISPI_IOPORT_INDEX, idx);
    return inw(VBE_DISPI_IOPORT_DATA);
}
 
bool vbe_available(void) {
	uint32_t val = vbe_read_reg(VBE_DISPI_INDEX_ID);
	return (val == VBE_DISPI_ID0 ||
			val == VBE_DISPI_ID1 ||
			val == VBE_DISPI_ID2 ||
			val == VBE_DISPI_ID3 ||
			val == VBE_DISPI_ID4 ||
			val == VBE_DISPI_ID5);
}
 
void vbe_set_video_mode(unsigned int width, unsigned int height, unsigned int depth, bool use_lfb, bool clear_vmem) {
	if (!vbe_available()) {
		printk("vbe_set_video_mode() failed: Bochs VBE unavailable\n");
	}

    vbe_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write_reg(VBE_DISPI_INDEX_XRES, width);
    vbe_write_reg(VBE_DISPI_INDEX_YRES, height);
    vbe_write_reg(VBE_DISPI_INDEX_BPP, depth);
    vbe_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED  |
					(use_lfb	? VBE_DISPI_LFB_ENABLED : 0) |
					(clear_vmem ? 0 : VBE_DISPI_NOCLEARMEM));
}
 
void vbe_set_bank(unsigned short bank_num) {
	static int current_bank = 0;
	if (bank_num == current_bank) return;

	current_bank = bank_num;
    vbe_write_reg(VBE_DISPI_INDEX_BANK, bank_num);
}
