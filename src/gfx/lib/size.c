#include "size.h"

Size size_make(int w, int h) {
	Size size;
	size.width = w;
	size.height = h;
	return size;
}

Size size_zero() {
	return size_make(0, 0);
}
