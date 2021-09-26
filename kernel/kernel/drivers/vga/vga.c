#include "vga.h"
#include <gfx/lib/gfx.h>
#include <kernel/kernel.h>

void vga_screen_refresh(Screen* screen) {
	write_screen(screen);
}

Screen* switch_to_vga() {
	Deprecated();
	return NULL;
}
