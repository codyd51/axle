#ifndef GUI_UTILS_H
#define GUI_UTILS_H

#include "gui_elem.h"
#include <agx/lib/ca_layer.h>

void draw_diagonal_insets(ca_layer* layer, Rect outer, Rect inner, Color c, uint32_t width);
const char* rect_print(Rect r);

#endif