#include <kernel/kernel.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/rtc/clock.h>

#include <gfx/lib/shapes.h>
#include <gfx/lib/view.h>
#include <gfx/lib/gfx.h>

#include <user/xserv/xserv.h>

#include <std/memory.h>
#include <std/timer.h>

#include "vesa.h"

Window* create_window_int(Rect frame, bool root);

//sets up VESA for mode
Screen* switch_to_vesa(uint32_t vesa_mode, bool create) {
	Deprecated();
	return NULL;
}
