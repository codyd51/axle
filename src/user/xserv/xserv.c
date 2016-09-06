#include "xserv.h"
#include <kernel/drivers/mouse/mouse.h>
#include <stddef.h>
#include <std/math.h>
#include <std/panic.h>
#include <std/std.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <gfx/lib/shader.h>

//has the screen been modified this refresh?
static char dirtied = 0;
static volatile Window* active_window;

Window* containing_window_int(Screen* screen, View* v) {
	//find root window
	View* view = v;
	while (view->superview) {
		view = view->superview;
	}

	if (screen->window->title_view == view || screen->window->content_view == view) return screen->window;
	//traverse view hierarchy, find window which has view as its title or content view
	for (unsigned i = 0; i < screen->window->subviews->size; i++) {
		Window* window = (Window*)array_m_lookup(screen->window->subviews, i);
		if (window->title_view == view || window->content_view == view) return window;
		for (unsigned j = 0; j < window->subviews->size; j++) {
			Window* subwindow = (Window*)array_m_lookup(window->subviews, j);
			if (subwindow->title_view == view || subwindow->content_view == view) return subwindow;
		}
	}
	return screen->window;
}

Rect convert_rect(Rect outer, Rect inner) {
	Rect ret;
	ret.origin.x = inner.origin.x + outer.origin.x;
	ret.origin.y = inner.origin.y + outer.origin.y;
	ret.size.width = MIN(inner.size.width, outer.size.width);
	ret.size.height = MIN(inner.size.height, outer.size.height);
	return ret;
}

Rect convert_frame(View* view, Rect frame) {
	if (!view) return frame;

	Rect ret = convert_rect(view->frame, frame);
	return ret;
}

Rect absolute_frame(Screen* screen, View* view) {
	Rect ret = view->frame;
	//find root view
	View* v = view;
	while (v->superview) {
		v = v->superview;
		ret = convert_frame(v, ret);
	}

	//find containing window
	Window* win = containing_window_int(screen, v);
	ASSERT(win, "couldn't find container window!");

	return convert_rect(win->frame, ret);
}

void draw_bmp(Screen* screen, Bmp* bmp) {
	View* superview = bmp->superview;
	//don't assert as the mouse cursor doesn't have a superview
	//TODO figure out workaround that works long-term
	//ASSERT(superview, "bmp had no superview!");
	if (!superview) {
		//printf_err("bmp had no superview!");
		superview = screen->window->content_view;
	}

//	if (!bmp || !bmp->needs_redraw) return;
//	if (superview && !superview->needs_redraw) return;

	bmp->needs_redraw = 1;
	dirtied = 1;

	Rect frame = absolute_frame(screen, (View*)bmp);
	frame.size.width = MIN(frame.size.width, bmp->raw_size.width);
	frame.size.height = MIN(frame.size.height, bmp->raw_size.height);

	int row_min = 0;
	if (frame.origin.x < 0) {
		row_min = -frame.origin.x;
	}
	int row_max = frame.size.width;
	if (frame.size.width + frame.origin.x > screen->window->frame.size.width) {
		row_max -= abs(frame.size.width + frame.origin.x - screen->window->frame.size.width);
	}
	int col_min = 0;
	if (frame.origin.y < 0) {
		col_min = -frame.origin.y;
	}
	int col_max = frame.size.height;
	if (frame.size.height + frame.origin.y > screen->window->frame.size.height) {
		col_max -= abs(frame.size.height + frame.origin.y - screen->window->frame.size.height);
	}

	int bpp = 24 / 8;
	int offset = frame.origin.x * bpp + (frame.origin.y + col_min) * screen->window->size.width * bpp + (row_min * bpp);

	int current_row_ptr = offset;
	Color* row = bmp->raw + row_min + (col_min * bmp->raw_size.width);
	for (int i = col_min; i < col_max; i++) {
		memcpy(screen->vmem + offset, row, row_max * bpp - row_min * bpp);
		row += bmp->raw_size.width;

		current_row_ptr += screen->window->size.width * bpp;
		offset = current_row_ptr;
	}
}

void draw_label(Screen* screen, Label* label) {
	View* superview = label->superview;
	//ASSERT(superview, "label had no superview!");
	
//	if (!label || !label->needs_redraw) return;
//	if (superview && !superview->needs_redraw) return;

	label->needs_redraw = 1;
	dirtied = 1;

	Rect frame = absolute_frame(screen, (View*)label);

	int idx = 0;
	char* str = label->text;
	int x = frame.origin.x;
	int y = frame.origin.y;
	while (str[idx] != NULL) {
		//go to next line if necessary
		if ((x + CHAR_WIDTH + CHAR_PADDING_W) > (frame.origin.x + frame.size.width) || str[idx] == '\n') {
			x = frame.origin.x;

			//quit if going to next line would exceed view bounds
			if ((y + CHAR_WIDTH + CHAR_PADDING_H) > (frame.origin.y + frame.size.height)) break;
			
			y += CHAR_HEIGHT + CHAR_PADDING_H;
		}

		draw_char(screen, str[idx], x, y, label->text_color);
		
		x += CHAR_WIDTH + CHAR_PADDING_W;

		idx++;
	}

	label->needs_redraw = 0;
}

void draw_view(Screen* screen, View* view) {
	View* superview = view->superview;
	Window* superwindow = containing_window_int(screen, view);

	/*
	if (!view || !view->needs_redraw) return;
	if (superview && !superview->needs_redraw) return;
	if (superwindow && !superwindow->needs_redraw) return;
	*/

	//inform subviews that we're being redrawn
	view->needs_redraw = 1;
	dirtied = 1;
	
	Rect frame = absolute_frame(screen, view);

	//fill view with its background color
	draw_rect(screen, frame, view->background_color, THICKNESS_FILLED);

	//draw any labels this view has
	for (unsigned i = 0; i < view->labels->size; i++) {
		Label* label = (Label*)array_m_lookup(view->labels, i);
		draw_label(screen, label);
	}

	//draw any bmps this view has
	for (unsigned i = 0; i < view->bmps->size; i++) {
		Bmp* bmp = (Bmp*)array_m_lookup(view->bmps, i);
		draw_bmp(screen, bmp);
	}

	//draw shaders last
	for (unsigned i = 0; i < view->shaders->size; i++) {
		Shader* s = (Shader*)array_m_lookup(view->shaders, i);
		draw_shader(screen, s);
	}

	//draw each subview of this view
	for (unsigned i = 0; i < view->subviews->size; i++) {
		View* subview = (View*)array_m_lookup(view->subviews, i);
		draw_view(screen, subview);
	}

	view->needs_redraw = 0;
}

void draw_window(Screen* screen, Window* window) {
//	if (!window->needs_redraw && !window->content_view->needs_redraw && !window->title_view->needs_redraw) return;

	window->needs_redraw = 1;
	dirtied = 1;

	//paint window
	draw_rect(screen, window->frame, window->border_color, window->border_width);
	
	//only draw a title bar if title_view exists
	if (window->title_view) {
		//update title label of window
		Label* title_label = (Label*)array_m_lookup(window->title_view->labels, 0);
		title_label->text = window->title;
		draw_view(screen, window->title_view);
	}

	//only draw the content view if content_view exists
	if (window->content_view) {
		draw_view(screen, window->content_view);

		//draw dividing border between window border and other content
		if (window->border_width) {
			//outer border
			Rect outer_border = rect_make(absolute_frame(screen, (View*)window).origin, size_make(absolute_frame(screen, (View*)window).size.width, absolute_frame(screen, (View*)window).size.height));
			draw_rect(screen, outer_border, color_black(), 1);
			
			//inner border
			Rect inner_border = rect_make(absolute_frame(screen, window->content_view).origin, size_make(absolute_frame(screen, window->content_view).size.width, absolute_frame(screen, window->content_view).size.height));
			draw_rect(screen, inner_border, color_gray(), 1);
		}
	}

	window->needs_redraw = 0;
}

void add_taskbar(Screen* screen) {
	Size taskbar_size = size_make(screen->window->frame.size.width, screen->window->frame.size.height * 0.045);
	Rect border_r = rect_make(point_make(0, 0), size_make(taskbar_size.width, 5));
	taskbar_size.height -= border_r.size.height;

	Coordinate taskbar_origin = point_make(0, screen->window->frame.size.height - taskbar_size.height + border_r.size.height);
	View* taskbar_view = create_view(rect_make(taskbar_origin, taskbar_size));
	taskbar_view->background_color = color_make(245, 120, 80);
	add_subview(screen->window->content_view, taskbar_view);

	//add top 'border' to taskbar
	//TODO add window border API
	View* border = create_view(border_r);
	border->background_color = color_make(200, 80, 245);
	add_subview(taskbar_view, border);
	
	//inner border seperating top and bottom of taskbar
	View* inner_border = create_view(rect_make(point_make(0, border_r.size.height), size_make(screen->window->frame.size.width, 1)));
	inner_border->background_color = color_make(50, 50, 50);
	add_subview(taskbar_view, inner_border);

	Rect usable = rect_make(point_make(0, border_r.origin.y + border_r.size.height), size_make(taskbar_view->frame.size.width, taskbar_view->frame.size.height - border_r.size.height));
	Coordinate name_label_origin = point_make(taskbar_view->frame.size.width * 0.925, usable.origin.y + (usable.size.height / 2) - (CHAR_HEIGHT / 2));
	Rect label_rect = rect_make(name_label_origin, size_make(taskbar_size.width - name_label_origin.x, taskbar_size.height));
	Label* name_label = create_label(label_rect, "axle OS");
	add_sublabel(taskbar_view, name_label);
}

void add_status_bar(Screen* screen) {
	Rect status_bar_r = rect_make(point_make(0, 0), size_make(screen->window->content_view->frame.size.width, screen->window->frame.size.height * 0.03));
	View* status_bar = create_view(status_bar_r);
	status_bar->background_color = color_make(150, 150, 150);
	add_subview(screen->window->content_view, status_bar);

	//border
	View* border = create_view(rect_make(point_make(0, status_bar_r.size.height - 1), size_make(status_bar_r.size.width, 1)));
	border->background_color = color_purple();
	add_subview(status_bar, border);
}

void draw_desktop(Screen* screen) {
	//paint root desktop
	draw_window(screen, screen->window);
	
	//paint every child window
	for (unsigned i = 0; i < screen->window->subviews->size; i++) {
		Window* win = (Window*)(array_m_lookup(screen->window->subviews, i));
		draw_window(screen, win);
	}
}

void desktop_setup(Screen* screen) {
	//set up background image
	Bmp* background = load_bmp(screen->window->content_view->frame, "windows-xp.bmp");
	add_bmp(screen->window->content_view, background);
	add_status_bar(screen);
	add_taskbar(screen);
}

void draw_cursor(Screen* screen) {
	static Bmp* cursor;
	if (!cursor) {
		cursor = load_bmp(rect_make(point_zero(), size_make(30, 30)), "cursor.bmp");
		cursor->frame.size = cursor->raw_size;
	}
	cursor->frame.origin = mouse_point();
	//we do not call add_bmp on the cursor
	//we draw it manually to ensure it is always above all other content
	draw_bmp(screen, cursor);
}

char xserv_draw(Screen* screen) {
	screen->finished_drawing = 0;

	dirtied = 0;
	draw_desktop(screen);
	draw_cursor(screen);

	screen->finished_drawing = 1;

	char ret = dirtied;
	dirtied = 0;
	return ret;
}

static Window* window_containing_point(Screen* screen, Coordinate p) {
	//traverse window hierarchy, starting with the topmost window
	for (int i = screen->window->subviews->size - 1; i >= 0; i--) {
		Window* w = (Window*)array_m_lookup(screen->window->subviews, i);
		//TODO implement rect_intersects
		if (p.x >= w->frame.origin.x && p.y >= w->frame.origin.y && p.x - w->frame.origin.x <= w->frame.size.width && p.y - w->frame.origin.y <= w->frame.size.height) {
			return w;
		}
	}
	//wasn't in any subwindows
	//point must be within root window
	//TODO should we check anyways?
	return screen->window;
}

static void process_mouse_events(Screen* screen) {
	static Window* grabbed_window;
	static Coordinate last_mouse_pos;

	//get mouse events
	uint8_t events = mouse_events();
	Coordinate p = mouse_point();

	//0th bit is left mouse button
	bool left = events & 0x1;
	if (left) {
		if (!grabbed_window) {
			//find the window that got this click
			Window* owner = window_containing_point(screen, p);
			grabbed_window = owner;

			//don't move root window! :p
			if (grabbed_window != screen->window) {
				//bring click owner to forefront
				//TODO place this in its own function?
				remove_subwindow(screen->window, grabbed_window);
				add_subwindow(screen->window, owner);

				active_window = grabbed_window;
			}
		}
		if (&last_mouse_pos != NULL && grabbed_window != screen->window) {
			//move this window by the difference between current mouse position and last mouse position
			grabbed_window->frame.origin.x -= (last_mouse_pos.x - p.x);
			grabbed_window->frame.origin.y -= (last_mouse_pos.y - p.y);

			//ensure we don't exceed screen bounds
			//selected_window->frame.origin.x = MAX(selected_window->frame.origin.x, 0);
			//selected_window->frame.origin.y = MAX(selected_window->frame.origin.y, 0);
		}
	}
	else {
		//click event ended, release window
		grabbed_window = NULL;
	}
	last_mouse_pos = p;
}

static Label* fps;
void xserv_refresh(Screen* screen) {
	if (!screen->finished_drawing) return;

	//check if there are any keys pending
	char ch;
	if ((ch = kgetch())) {
		if (ch == 'q') {
			//quit xserv
			//gfx_teardown(screen);
			xserv_pause();
			return;
		}
	}

	double time_start = time();
	xserv_draw(screen);
	double frame_time = (time() - time_start) / 1000.0;

	//update frame time tracker 
	char buf[32];
	itoa(frame_time * 1000, &buf);
	strcat(buf, " ms/frame");
	fps->text = buf;
	draw_label(screen, fps);

	//draw rect to indicate whether the screen was dirtied this frame
	//red indicates dirtied, green indicates clean
	Rect dirtied_indicator = rect_make(point_make(0, screen->window->size.height - 25), size_make(25, 25));
	draw_rect(screen, dirtied_indicator, (dirtied ? color_red() : color_green()), THICKNESS_FILLED);

	//handle mouse events
	process_mouse_events(screen);
	
	write_screen(screen);
}

void xserv_pause() {
	switch_to_text();
}

void xserv_resume() {
	switch_to_vesa(0x118, false);
}

void xserv_temp_stop(uint32_t pause_length) {
	xserv_pause();
	//sleep(pause_length);
	xserv_resume();
}

void xserv_init_late() {
	//switch to VESA for x serv
	Screen* screen = switch_to_vesa(0x118, true);
	
	set_frame(screen->window->title_view, rect_make(point_make(0, 0), size_make(0, 0)));
	set_frame(screen->window->content_view, screen->window->frame);
	set_border_width(screen->window, 0);
	desktop_setup(screen);

	//add FPS tracker
	fps = create_label(rect_make(point_make(3, 3), size_make(300, 50)), "FPS counter");
	fps->text_color = color_black();
	add_sublabel(screen->window->content_view, fps);

	//test_xserv(screen);

	while (1) {
		xserv_refresh(screen);
		//sys_yield();
	}
}

void xserv_init() {
	add_process(create_process((uint32_t)xserv_init_late));
    sleep(500);
    xserv_pause();
}
