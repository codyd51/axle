#ifndef GFX_H
#define GFX_H

typedef struct framebuffer_info {
    uint32_t type;
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t bytes_per_pixel;
    uint32_t size;
} framebuffer_info_t;

#endif