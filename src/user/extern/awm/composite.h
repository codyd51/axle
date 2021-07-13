#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <agx/lib/shapes.h>

void compositor_init(void);
void compositor_queue_rect_to_redraw(Rect update_rect);
void compositor_queue_rect_difference_to_redraw(Rect bg, Rect fg);
void compositor_render_frame(void);

#endif