#ifndef AWM_UTILS_H
#define AWM_UTILS_H

#include <stdlibadd/array.h>

#include <agx/lib/shapes.h>

#include <libimg/libimg.h>
#include <file_manager/file_manager_messages.h>

uint32_t ms_since_boot();

Point point_translate(Point p, Rect r);
Size screen_resolution(void);
uint8_t screen_bytes_per_pixel(void);
uint32_t screen_pixels_per_scanline(void);

bool rect_contains_rect(Rect a, Rect b);
array_t* rect_diff(Rect bg, Rect fg);

void rect_add(array_t* arr, Rect r);
array_t* update_occlusions(array_t* free_areas, Rect exclude_rect);

image_t* load_image(const char* image_name);

// Friend functions for awm
void _set_screen_resolution(Size);
void _set_screen_bytes_per_pixel(uint8_t);
void _set_screen_pixels_per_scanline(uint32_t);

#endif