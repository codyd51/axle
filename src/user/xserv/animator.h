#ifndef ANIMATOR_H
#define ANIMATOR_H

#include <gfx/lib/window.h>
#include <gfx/lib/gfx.h>

typedef enum animation_type {
	ALPHA_ANIM = 0,
	POS_ANIM,
	COLOR_ANIM,
} animation_type;

typedef struct ca_animation ca_animation;
typedef void (*animation_update)(Window* window, ca_animation* anim, float frame_time);
typedef struct ca_animation {
	animation_type type;

	float alpha_to;
	Coordinate pos_to;
	Color color_to;

	animation_update update;
	event_handler finished_handler;

	float duration;
	uint32_t end_date;
} ca_animation;

ca_animation* create_animation(animation_type type, void* to, float duration);
void add_animation(Window* window, ca_animation* anim);
void finalize_animation(Window* window, ca_animation* anim);
void process_animations(Window* window, float frame_time);
void update_all_animations(Screen* screen, float frame_time);

#endif
