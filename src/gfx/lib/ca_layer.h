#ifndef CA_LAYER_H
#define CA_LAYER_H

#include <std/std_base.h>
#include <stdint.h>
#include "rect.h"

__BEGIN_DECLS

typedef struct ca_layer_t {
       	Size size;
       	uint8_t* raw;
} ca_layer;

struct ca_layer_t* create_layer(Size size);

__END_DECLS

#endif
