#ifndef LIBIMG_H
#define LIBIMG_H

#include <agx/lib/shapes.h>

typedef struct image_bmp {
    Size size;
    uint32_t bit_count;
    uint8_t* pixel_data;
} image_bmp_t;

image_bmp_t* image_parse_bmp(uint32_t size, uint8_t* data);
void image_free(image_bmp_t* image);
void image_render_to_layer(image_bmp_t* image, ca_layer* dest, Rect frame);

#endif
