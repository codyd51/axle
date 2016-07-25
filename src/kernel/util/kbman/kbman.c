#include "kbman.h"
#include <kernel/drivers/terminal/terminal.h>

void kbman_process(char c) {
	if (c == 0x48) {
		term_scroll(TERM_SCROLL_UP);	
	}
	else if (c == 0x50) {
		term_scroll(TERM_SCROLL_DOWN);
	}
}
