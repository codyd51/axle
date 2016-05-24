#ifndef VIEW_H
#define VIEW_H

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct Size {
	int width;
	int height;
} Size;

typedef struct rect {
	coordinate origin;
	Size size;
} rect;

// dynamic_array
typedef void* type_t;

typedef struct dynamic_array {
	type_t* array;
	uint32_t used;
	uint32_t size;
} dynamic_array;

dynamic_array* dynamic_array_create(uint32_t initialSize);
void dynamic_array_insert(dynamic_array *array, type_t element);
void dynamic_array_free(dynamic_array *array);

// view and window


typedef struct view {
	rect frame;
	uint32_t zIndex;
	struct view *superview;
	dynamic_array *subviews;
} view;

typedef struct window {
	Size size;
	dynamic_array *subwindows;
	struct view view;
} window;

#endif
