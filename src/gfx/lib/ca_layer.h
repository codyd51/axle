#ifndef CA_LAYER_H
#define CA_LAYER_H

#include <std/std_base.h>
#include <stdint.h>
#include "rect.h"

__BEGIN_DECLS

typedef struct ca_layer_t {
       	Size size; //width/height in pixels
       	uint8_t* raw; //raw RGB values backing this layer
		float alpha; //transparency value bounded to continuous range [0..1]
} ca_layer;

//initialize layer with a given size
struct ca_layer_t* create_layer(Size size);

//free all resources associated with a layer
//no fields of 'layer' should be accessed after this call
void layer_teardown(ca_layer* layer);

//blit RGB contents of 'src' onto 'dest'
//automatically switches to compositing if 'dest' needs ot be alpha blended
//only copies pixels from the rectangle bounded by 'src_frame'
//only copies pixels into the rectangle bounded by 'dest_frame'
void blit_layer(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame);

//create a copy of layer pointed to by src
//only copies pixels bounded by the rectangle 'frame'
ca_layer* layer_snapshot(ca_layer* src, Rect frame);

__END_DECLS

#endif
