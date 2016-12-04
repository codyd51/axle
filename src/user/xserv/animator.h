#ifndef ANIMATOR_H
#define ANIMATOR_H

#include <gfx/lib/window.h>
#include <gfx/lib/gfx.h>

typedef enum animation_type {
	ALPHA_ANIM = 0,
} animation_type;

typedef struct ca_animation {
	animation_type type;
	float to_val;
	float duration;
} ca_animation;

ca_animation* create_animation(animation_type type, float to_val, float duration);
void add_animation(Window* window, ca_animation* anim);
void finalize_animation(Window* window, ca_animation* anim);
void process_animations(Window* window, float frame_time);
void update_all_animations(Screen* screen, float frame_time);

#endif
