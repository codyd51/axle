
#include "putpixel.h"
#include "../math.h"
#include "screen.h"
#include <assert.h>

__attribute__((always_inline))
inline void putpixel_alpha(ca_layer* layer, int x, int y, Color color, int alpha) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;
	alpha = MAX(alpha, 0);
	alpha = MIN(alpha, 255);

	Screen* screen = gfx_screen();
	if (!screen) assert("Cannot call putpixel() without a screen");

	int offset = (x * screen->bytes_per_pixel) + (y * layer->size.width * screen->bytes_per_pixel);
	for (int i = 0; i < 3; i++) {
		// Alpha blend: ((alpha*(src-dest))>>8)+dest
		// Pixels are written in BGR, not RGB
		// Thus, flip color order when reading a source color-byte
		int src = color.val[screen->bytes_per_pixel - i - 1];
		int dst = layer->raw[offset + i];
		int composited = ((alpha * (src - dst)) >> 8) + dst;
		layer->raw[offset + i] = composited;
	}
}

__attribute__((always_inline))
inline void putpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	Screen* screen = gfx_screen();
	if (!screen) assert("Cannot call putpixel() without a screen");

	uint32_t offset = (x * screen->bytes_per_pixel) + (y * layer->size.width * screen->bytes_per_pixel);
	for (uint32_t i = 0; i < 3; i++) {
		// Pixels are written in BGR, not RGB
		// Thus, flip color order when reading a source color-byte
		layer->raw[offset + i] = color.val[screen->bytes_per_pixel - i - 1];
	}
}
