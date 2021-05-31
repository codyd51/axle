#ifndef LIBIMG_H
#define LIBIMG_H

#include <agx/lib/shapes.h>

typedef enum image_type {
    IMAGE_BITMAP = 0,
    IMAGE_JPEG = 1
} image_type_t;

typedef struct image {
    image_type_t type;
    Size size;
    uint32_t bit_count;
    uint8_t* pixel_data;
} image_t;

// Auto-detects image format
image_t* image_parse(uint32_t size, uint8_t* data);
// Format-specific image parsers
image_t* image_parse_bmp(uint32_t size, uint8_t* data);
image_t* image_parse_jpeg(uint32_t size, uint8_t* data);

void image_free(image_t* image);
void image_render_to_layer(image_t* image, ca_layer* dest, Rect frame);

#endif
