#ifndef BMP_H
#define BMP_H

#include <std/std_base.h>
#include <stdint.h>
#include "color.h"
#include "rect.h"
#include "ca_layer.h"

__BEGIN_DECLS

typedef struct bitmap {
	//common
	Rect frame;
	char needs_redraw;
	ca_layer* layer;
	struct view* superview;
} Bmp;

Bmp* create_bmp(Rect frame, ca_layer* layer);
Bmp* load_bmp(Rect frame, char* filename);

__END_DECLS

#endif
