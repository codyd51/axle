#ifndef SIZE_H
#define SIZE_H

typedef struct size {
	int width;
	int height;
} Size;

Size size_make(int w, int h);
Size size_zero();

#endif
