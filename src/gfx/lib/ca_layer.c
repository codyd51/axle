#include "ca_layer.h"

ca_layer* create_layer(Size size) {
	ca_layer* ret = (ca_layer*)kmalloc(sizeof(ca_layer));
	ret->size = size;
	ret->raw = (uint8_t*)kmalloc(size.width * size.height * gfx_bpp());
	return ret;
}

