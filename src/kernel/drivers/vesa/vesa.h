#ifndef VESA_H
#define VESA_H

#include "gfx/lib/gfx.h"
#include <gfx/lib/color.h>
#include <kernel/drivers/vbe/vbe.h>

Screen* switch_to_vesa(uint32_t mode, bool create);

#endif
