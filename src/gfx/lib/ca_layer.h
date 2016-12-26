#ifndef CA_LAYER_H
#define CA_LAYER_H

#include <std/std_base.h>
#include <stdint.h>
#include "rect.h"

__BEGIN_DECLS

typedef struct ca_layer_t {
       	Size size;
       	uint8_t* raw;
		float alpha;
} ca_layer;

struct ca_layer_t* create_layer(Size size);
void layer_teardown(ca_layer* layer);
void blit_layer(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame);
ca_layer* layer_snapshot(ca_layer* src, Rect frame);

__END_DECLS

#endif
