#include "shader.h"
#include <std/kheap.h>
#include <std/memory.h>
//#include "view.h"
#include <user/xserv/xserv.h>

Shader* create_shader(Vec2d dir) {
	Shader* s = (Shader*)kmalloc(sizeof(Shader));

	s->dir = dir;
	return s;
}

Shader* compute_shader(Shader* s) {
	if (s->raw) {
		kfree(s->raw);
	}
	Rect frame = s->superview->frame;
	s->raw = (Color*)kmalloc(sizeof(Color) * frame.size.width * frame.size.height);
	for (int y = 0; y < frame.size.height; y++) {
		for (int x = 0; x < frame.size.width; x++) {
			int idx = (y * frame.size.width) + x;
			double y_pc = y / (double)frame.size.height;
			double x_pc = x / (double)frame.size.width;

			Color px;
			px.val[0] = (1 - y_pc) * 255;
			px.val[1] = (1 - x_pc) * 255;
			px.val[2] = y_pc * 255;
			s->raw[idx] = px;
		}
	}
	return s;
}

void draw_shader(Screen* screen, Shader* s) {
	Rect frame = absolute_frame(screen, s->superview);

	int bpp = 24 / 8;
	int offset = (frame.origin.x * bpp) + (frame.origin.y * screen->window->size.width * bpp);
	uint8_t* shader_offset = (uint8_t*)s->raw;
	for (int i = 0; i < frame.size.width; i++) {
		memadd(screen->window->layer->raw + offset, shader_offset, frame.size.width * bpp);
		offset += (screen->window->size.width * bpp);
		shader_offset += frame.size.width * bpp;
	}
}

void shader_teardown(Shader* shader) {
	if (!shader) return;

	kfree(shader);
}

