#ifndef AWM_EFFECTS_H
#define AWM_EFFECTS_H

#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>

#include "window.h"

void radial_gradiant(
    ca_layer* layer, 
    Size gradient_size, 
    Color c1, 
    Color c2, 
    int x1, 
    int y1, 
    float r
);

void draw_window_backdrop_segments(ca_layer* dest, user_window_t* window, int segments);

#endif
