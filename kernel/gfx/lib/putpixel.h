#ifndef PUTPIXEL_H
#define PUTPIXEL_H

#include <std/std.h>
#include "ca_layer.h"
#include "color.h"

// Draw a pixel within the provided layer
void putpixel(ca_layer* layer, int x, int y, Color color);

// Draw a pixel within the provided layer, and alpha-blend with the existing layer contents
void putpixel_alpha(ca_layer* layer, int x, int y, Color color, int alpha);

#endif