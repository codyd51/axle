#include "animator.h"
#include <std/math.h>
#include <std/kheap.h>
#include <kernel/drivers/rtc/clock.h>

void add_animation(Window* window, ca_animation* anim) {
	array_m_insert(window->animations, anim);
	anim->end_date = time() + (anim->duration * 1000);
}

void finalize_animation(Window* window, ca_animation* anim) {
	//ensure property is set to final state
	//(just in case there was an issue with timing)
	switch (anim->type) {
		case ALPHA_ANIM:
			set_alpha((View*)window, anim->alpha_to);
			break;
		case POS_ANIM:
			window->frame.origin = anim->pos_to;
			break;
		case COLOR_ANIM:
		default:
			window->content_view->background_color = anim->color_to;
			break;
	}
	
	event_handler finished = anim->finished_handler;
	if (finished) {
		finished(window, NULL);
	}

	mark_needs_redraw((View*)window);

	array_m_remove(window->animations, array_m_index(window->animations, anim));
	kfree(anim);
}

void update_alpha_anim(Window* window, ca_animation* anim, float frame_time) {
	float new;
	//add or subtract alpha?
	//0: sub, 1: add
	bool dir = (anim->alpha_to >= window->layer->alpha);
	if (dir) {
		new = window->layer->alpha + (frame_time * (1 / anim->duration));
	}
	else {
		new = window->layer->alpha - (frame_time * (1 / anim->duration));
	}

	new = MIN(new, 1.0);
	set_alpha((View*)window, new);
}

void update_pos_anim(Window* window, ca_animation* anim, float frame_time) {
	float step = frame_time * 2 * (1 / anim->duration);
	window->frame.origin.x = (int)lerp(window->frame.origin.x, anim->pos_to.x, 1.0 * step);
	window->frame.origin.y = (int)lerp(window->frame.origin.y, anim->pos_to.y, 1.0 * step);
}

void update_color_anim(Window* window, ca_animation* anim, float frame_time) {
	float step = frame_time * 2 * (1 / anim->duration);
	Color current = window->content_view->background_color;
	current.val[0] = (uint8_t)lerp(current.val[0], anim->color_to.val[0], 1.0 * step);
	current.val[1] = (uint8_t)lerp(current.val[1], anim->color_to.val[1], 1.0 * step);
	current.val[2] = (uint8_t)lerp(current.val[2], anim->color_to.val[2], 1.0 * step);

	window->content_view->background_color = current;
	mark_needs_redraw((View*)window);
}

void process_animations(Window* window, float frame_time) {
	uint32_t now = time();
	for (int i = 0; i < window->animations->size; i++) {
		ca_animation* anim = array_m_lookup(window->animations, i);

		if (now >= anim->end_date) {
			finalize_animation(window, anim);
		}
		else {
			animation_update handler = anim->update;
			if (handler) {
				handler(window, anim, frame_time);
			}
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

ca_animation* create_animation(animation_type type, void* to, float duration) {
	ca_animation* ret = kmalloc(sizeof(ca_animation));
	ret->type = type;

	switch (type) {
		case ALPHA_ANIM:
			ret->alpha_to = *(float*)to;
			ret->update = &update_alpha_anim;
			break;
		case POS_ANIM:
			ret->pos_to = *(Coordinate*)to;
			ret->update = &update_pos_anim;
			break;
		case COLOR_ANIM:
			ret->color_to = *(Color*)to;
			ret->update = &update_color_anim;
			break;
	}

	ret->duration = duration;

	return ret;
}

