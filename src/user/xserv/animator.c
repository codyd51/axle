#include "animator.h"
#include <std/math.h>

ca_animation* create_animation(animation_type type, float to_val, float duration) {
	ca_animation* ret = kmalloc(sizeof(ca_animation));
	ret->type = type;
	ret->to_val = to_val;
	ret->duration = duration;
	return ret;
}

void add_animation(Window* window, ca_animation* anim) {
	array_m_insert(window->animations, anim);
}

void finalize_animation(Window* window, ca_animation* anim) {
	array_m_remove(window->animations, array_m_index(window->animations, anim));
	kfree(anim);
}

void process_animations(Window* window, float frame_time) {
	for (int i = 0; i < window->animations->size; i++) {
		ca_animation* anim = array_m_lookup(window->animations, i);
		
				float new = window->layer->alpha + (frame_time * (1 / anim->duration));
		switch (anim->type) {
			case ALPHA_ANIM:
			default:
				new = MIN(new, 1.0);
				if (new >= anim->to_val) {
					new = MIN(new, anim->to_val);
					finalize_animation(window, anim);
				}
				set_alpha((View*)window, new);
				break;
		}
	}
}

void update_all_animations(Screen* screen, float frame_time) {
	for (int i = 0; i < screen->window->subviews->size; i++) {
		Window* w = array_m_lookup(screen->window->subviews, i);
		if (w->animations->size) {
			process_animations(w, frame_time);
		}
	}
}
