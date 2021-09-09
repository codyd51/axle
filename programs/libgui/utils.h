#ifndef GUI_UTILS_H
#define GUI_UTILS_H

#include "gui_elem.h"
#include "gui_layer.h"

uint32_t ms_since_boot(void);

void draw_diagonal_insets(gui_layer_t* layer, Rect outer, Rect inner, Color c, uint32_t width);
const char* rect_print(Rect r);
void rect_add(array_t* arr, Rect r);

#endif