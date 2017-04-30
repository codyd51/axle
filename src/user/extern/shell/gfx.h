#include <stdint.h>

typedef struct point {
	int x;
	int y;
} Point;

typedef struct size {
	int width;
	int height;
} Size;

typedef struct rect {
	Point origin;
	Size size;
} Rect;

typedef struct color {
	uint8_t val[4];
} Color;

typedef struct window {
	//common
	Rect frame;
	char needs_redraw;
	void* layer;
	struct window* superview;
	void* subviews;

	Size size;
	char* title;
	void* title_view;
	void* content_view;
	Color border_color;
	int border_width;
	void* animations;
	void* teardown_handler;
	void* redraw_handler;

	uint32_t last_draw_timestamp;
} Window;
