#include <unistd.h>
#include <stdint.h>

#include <libutils/assert.h>
#include <libutils/array.h>

#include <libagx/lib/shapes.h>

#include "window.h"
#include "awm_internal.h"
#include "composite.h"

array_t* _g_screen_rects_to_update_this_cycle = NULL;

/* Queue a composite for the provided rect over the entire desktop
    While compositing the frame, awm will determine what individual elements
    must be redrawn to composite the provided rectangle.
	These may include portions of windows, the desktop background, etc.
 */
void compositor_queue_rect_to_redraw(Rect update_rect) {
	if (update_rect.size.width == 0 || update_rect.size.height == 0) {
		// TODO(PT): Investigate how this happens? Trigger by quickly resizing a window to flood events
		//printf("Dropping update rect of zero height or width\n");
		return;
	}
	if (_g_screen_rects_to_update_this_cycle->size + 1 >= _g_screen_rects_to_update_this_cycle->max_size) {
		//printf("Dropping update rect because we've hit our max updates this cycle: (%d, %d), (%d, %d)\n", rect_min_x(update_rect), rect_min_y(update_rect), update_rect.size.width, update_rect.size.height);
		return;
	}
	Rect* r = calloc(1, sizeof(Rect));
	r->origin.x = update_rect.origin.x;
	r->origin.y = update_rect.origin.y;
	r->size.width = update_rect.size.width;
	r->size.height = update_rect.size.height;
	array_insert(_g_screen_rects_to_update_this_cycle, r);
}

/* Queue composites for the area of the bg rectangle that's not obscured by the fg rectangle
 */
void compositor_queue_rect_difference_to_redraw(Rect bg, Rect fg) {
	array_t* delta = rect_diff(bg, fg);
	for (int32_t i = delta->size - 1; i >= 0; i--) {
		Rect* r = array_lookup(delta, i);
		compositor_queue_rect_to_redraw(*r);
		free(r);
	}
	array_destroy(delta);
}

void compositor_init(void) {
	_g_screen_rects_to_update_this_cycle = array_create(256);
}

void compositor_render_frame(void) {
	ca_layer* desktop_background = desktop_background_layer();
	array_t* all_views = all_desktop_views();
	ca_layer* video_memory = video_memory_layer();
	ca_layer* physical_video_memory = physical_video_memory_layer();

	// Fetch remote layers for windows that have asked for a redraw
	windows_fetch_queued_windows();

	// Process rects that have been dirtied while processing other events
	for (int32_t i = 0; i < _g_screen_rects_to_update_this_cycle->size; i++) {
		Rect* rp = array_lookup(_g_screen_rects_to_update_this_cycle, i);
		Rect r = *rp;

		array_t* unobscured_region = array_create(256);
		rect_add(unobscured_region, r);

		// Handle the parts of the dirty region that are obscured by desktop views
		for (int32_t j = 0; j < all_views->size; j++) {
			view_t* view = array_lookup(all_views, j);
			// We can't occlude using a view if the view uses transparency 
			if (view->layer->alpha < 1.0) {
				continue;
			}
			if (!rect_intersects(view->frame, r)) {
				continue;
			}

			for (int32_t k = 0; k < view->drawable_rects->size; k++) {
				Rect* visible_region_ptr = array_lookup(view->drawable_rects, k);
				Rect visible_region = *visible_region_ptr;
				if (rect_intersects(visible_region, r)) {
					if (rect_contains_rect(visible_region, r)) {
						// The entire rect should be redrawn from this window
						rect_add(view->extra_draws_this_cycle, r);
						// And subtract the area of the rect from the region to update
						unobscured_region = update_occlusions(unobscured_region, r);
						break;
					}
					else {
						// This view needs to redraw the intersection of its visible rect and the update rect
						Rect intersection = rect_intersect(visible_region, r);
						rect_add(view->extra_draws_this_cycle, intersection);
						unobscured_region = update_occlusions(unobscured_region, intersection);
					}

					// If all the area of the region to update has been handled, 
					// stop iterating windows early
					if (!unobscured_region->size) {
						break;
					}
				}
			}
		}

		// Blit the regions that are not covered by windows with the desktop background layer
		for (int32_t j = unobscured_region->size - 1; j >= 0; j--) {
			Rect* bg_rect = array_lookup(unobscured_region, j);
			blit_layer(
				video_memory,
				desktop_background,
				*bg_rect,
				*bg_rect
			);
			array_remove(unobscured_region, j);
			free(bg_rect);
		}
		array_destroy(unobscured_region);
	}

	array_t* desktop_views_to_composite = desktop_views_ready_to_composite_array();
	draw_views_to_layer(desktop_views_to_composite, video_memory);
	draw_queued_extra_draws(all_views, video_memory);

	Rect mouse_rect = _draw_cursor(video_memory);

	// Blit everything we drew above to the memory-mapped framebuffer
	for (int32_t i = _g_screen_rects_to_update_this_cycle->size - 1; i >= 0; i--) {
		Rect* r = array_lookup(_g_screen_rects_to_update_this_cycle, i);
		blit_layer(
			physical_video_memory,
			video_memory,
			*r,
			*r
		);
		array_remove(_g_screen_rects_to_update_this_cycle, i);
		free(r);
	}

	complete_queued_extra_draws(all_views, video_memory, physical_video_memory);
	
	for (int32_t i = 0; i < desktop_views_to_composite->size; i++) {
		view_t* view = array_lookup(desktop_views_to_composite, i);
		for (int32_t j = 0; j < view->drawable_rects->size; j++) {
			Rect* r_ptr = array_lookup(view->drawable_rects, j);
			Rect r = *r_ptr;
			blit_layer(physical_video_memory, video_memory, r, r);
		}
	}
	blit_layer(physical_video_memory, video_memory, mouse_rect, mouse_rect);

	desktop_views_flush_queues();
	array_destroy(all_views);
}
