#include "animator.h"
#include <std/math.h>
#include <std/kheap.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/util/mutex/mutex.h>

void add_animation(Window* window, ca_animation* anim) {
	array_m_insert(window->animations, anim);
	anim->end_date = tick_count() + (anim->duration * 1000);
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

	//remove anim _before_ calling finalize handler
	//finalize handler may destroy window so we need to do this first
	array_m_remove(window->animations, array_m_index(window->animations, anim));
	
	event_handler finished = anim->finished_handler;
	if (finished) {
		printf("calling finished_handler %x for win %x\n", finished, window);
		finished(window, NULL);
	}

	kfree(anim);

	mark_needs_redraw((View*)window);
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
	float step = frame_time * (1.0 / anim->duration);
	window->frame.origin.x = (int)lerp(window->frame.origin.x, anim->pos_to.x, step);
	window->frame.origin.y = (int)lerp(window->frame.origin.y, anim->pos_to.y, step);
}

void update_color_anim(Window* window, ca_animation* anim, float frame_time) {
	float step = frame_time * (1.0 / anim->duration);
	Color current = window->content_view->background_color;
	current.val[0] = (uint8_t)lerp(current.val[0], anim->color_to.val[0], step);
	current.val[1] = (uint8_t)lerp(current.val[1], anim->color_to.val[1], step);
	current.val[2] = (uint8_t)lerp(current.val[2], anim->color_to.val[2], step);

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
			anim->duration -= frame_time;
		}
	}
}

void update_all_animations(Screen* screen, float frame_time) {
	for (int i = 0; i < screen->window->subviews->size; i++) {
		Window* w = array_m_lookup(screen->window->subviews, i);
		if (w->animations->size) {
			printk("processing %d animations for %x\n", w->animations->size, w);
			process_animations(w, frame_time);
		}
	}
}

ca_animation* create_animation(animation_type type, void* to, float duration) {
	ca_animation* ret = kmalloc(sizeof(ca_animation));
	memset(ret, 0, sizeof(ca_animation));
	ret->type = type;

	switch (type) {
		case ALPHA_ANIM:
			ret->alpha_to = *(float*)to;
			ret->update = &update_alpha_anim;
			break;
		case POS_ANIM:
			ret->pos_to = *(Point*)to;
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

