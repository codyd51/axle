#ifndef SHADER_H
#define SHADER_H

#include "gfx.h"

typedef struct shader {
	//common
	Rect frame;
	char needs_redraw;
	struct view* superview;

	Vec2d dir;
	Color* raw;
} Shader;

Shader* create_shader();
void shader_teardown(Shader* shader);

void draw_shader(Screen* screen, Shader* s);
Shader* compute_shader(Shader* s);

#endif
