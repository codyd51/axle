#include "xserv.h"
#include <kernel/drivers/mouse/mouse.h>
#include <stddef.h>
#include <std/math.h>
#include <kernel/assert.h>
#include <std/std.h>
#include <gfx/lib/gfx.h>
#include <kernel/syscall/sysfuncs.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/drivers/rtc/clock.h>
#include <tests/gfx_test.h>
#include <kernel/drivers/kb/kb.h>
#include "animator.h"
#include <gfx/lib/rect.h>
#include <kernel/util/unistd/exec.h>

Window* create_window_int(Rect frame, bool is_root_window);

//has the screen been modified this refresh?
static char dirtied = 0;
static volatile Window* active_window;
const int shadow_count = 4;

void xserv_quit(Screen* screen) {
	gfx_teardown(screen);
	resign_first_responder();
	_kill();
}
void launcher_button_clicked(Button* UNUSED(b)) {
	//launcher_invoke(point_make(20, rect_min_y(b->frame) - 200));
	//launcher_invoke(point_make(20, 20));
}

void add_taskbar(Screen* screen) {
	View* content = screen->window->content_view;
	Size taskbar_size = size_make(content->frame.size.width, content->frame.size.height * 0.045);
	Rect border_r = rect_make(point_make(0, 0), size_make(taskbar_size.width, 5));
	taskbar_size.height -= border_r.size.height;

	Point taskbar_origin = point_make(0, content->frame.size.height - taskbar_size.height + border_r.size.height);

	View* taskbar_view = create_view(rect_make(taskbar_origin, taskbar_size));
	taskbar_view->background_color = color_make(245, 120, 80);
	add_subview(content, taskbar_view);

	//add top 'border' to taskbar
	//TODO add window border API
	View* border = create_view(border_r);
	border->background_color = color_make(200, 80, 245);
	add_subview(taskbar_view, border);

	//inner border seperating top and bottom of taskbar
	View* inner_border = create_view(rect_make(point_make(0, border_r.size.height), size_make(content->frame.size.width, 1)));
	inner_border->background_color = color_make(50, 50, 50);
	add_subview(taskbar_view, inner_border);

	Rect usable = rect_make(point_make(0, border_r.origin.y + border_r.size.height), size_make(taskbar_view->frame.size.width, taskbar_view->frame.size.height - border_r.size.height));
	Point name_label_origin = point_make(taskbar_view->frame.size.width * 0.925, usable.origin.y + (usable.size.height / 2) - (CHAR_HEIGHT / 2));
	Rect label_rect = rect_make(name_label_origin, size_make(taskbar_size.width - name_label_origin.x, taskbar_size.height));
	Label* name_label = create_label(label_rect, "axle OS");
	add_sublabel(taskbar_view, name_label);

	//launcher button
	char* launcher_title = "Launcher";
	Button* launcher = create_button(rect_make(point_zero(), size_make(strlen(launcher_title) * CHAR_WIDTH + 20, taskbar_view->frame.size.height)), launcher_title);
	launcher->mousedown_handler = (event_handler)&launcher_button_clicked;
	add_button(taskbar_view, launcher);
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

static Point last_grabbed_window_pos;
void draw_window_shadow(Screen* screen, Window* window, Point new) {
	return;
	Point old = last_grabbed_window_pos;
	int actual_shadow_count = shadow_count;
	if (window->layer->alpha < 1.0) actual_shadow_count = 2.0;
	for (int i = 0; i < actual_shadow_count; i++) {
		int lerp_x = lerp(old.x, new.x, (1 / (float)actual_shadow_count) * i);
		int lerp_y = lerp(old.y, new.y, (1 / (float)actual_shadow_count) * i);
		/*
		if (abs(old.x - new.x) < 2 || abs(old.y - new.y) < 2) {
			continue;
		}
		*/
		Point shadow_loc = point_make(lerp_x, lerp_y);

		//draw snapshot of window
		blit_layer(screen->vmem, window->layer, rect_make(shadow_loc, window->layer->size), rect_make(point_zero(), window->layer->size));
	}
}

void draw_window_backdrop_segments(Screen* screen, Window* win, int segments) {
#define LEFT_EDGE 0x1
#define RIGHT_EDGE 0x2
#define BOTTOM_EDGE 0x4
	//draw gradient around bottom, left, and right edges of window
	//gives 'depth' to windows
	Rect draw = win->frame;
	int max_dist = 10;
	draw.origin.x -= 10;
	draw.size.width += max_dist * 2;
	draw.size.height += max_dist;

	Color col = color_black();
	//maximum alpha [0-255]
	int darkest = 200;

	for (int y = rect_min_y(draw); y < rect_max_y(draw); y++) {
		//past bottom of window?
		//only do bottom edge if that option is set
		if (!(segments & BOTTOM_EDGE)) {
			continue;
		}

		if (y < rect_min_y(win->frame) || y >= rect_max_y(win->frame)) {
			for (int x = rect_min_x(draw); x < rect_max_x(draw); x++) {
				int alpha = 0;
				//deal with corners
				if (x < rect_min_x(win->frame) || x >= rect_max_x(win->frame)) {
					int x_dist = 0;
					if (x < rect_min_x(win->frame)) {
						//left corner
						x_dist = rect_min_x(win->frame) - x;
					}
					else {
						//right corner
						x_dist = x - rect_max_x(win->frame);
					}

					int y_dist = 0;
					if (y < rect_max_y(win->frame)) {
						y_dist = rect_min_y(win->frame) - y;
					}
					else {
						y_dist = y - rect_max_y(win->frame);
					}

					//distance formula to get distance from corner
					float fact = (x_dist*x_dist) + (y_dist*y_dist);
					float norm = sqrt(fact);
					//invert intensity
					norm = max_dist - norm;

					alpha = (darkest / max_dist) * (norm);
				}
				//non-corner
				else {
					int y_dist = 0;
					if (y < rect_min_y(win->frame)) {
						y_dist = rect_min_y(win->frame) - y;
					}
					else {
						y_dist = (y - rect_max_y(win->frame));
					}
					alpha = (darkest / max_dist) * (max_dist - y_dist);
				}
				putpixel_alpha(screen->vmem, x, y, col, alpha);
			}
		}
		else {
			if (segments & LEFT_EDGE) {
				//left edge of window
				for (int x = rect_min_x(draw); x < rect_min_x(win->frame); x++) {
					int dist = (x - rect_min_x(draw));
					int alpha = (darkest / max_dist) * dist;
					putpixel_alpha(screen->vmem, x, y, col, alpha);
				}
			}
			if (segments & RIGHT_EDGE) {
				//right edge of window
				for (int x = rect_max_x(win->frame); x < rect_max_x(draw); x++) {
					int dist = (max_dist - (x - rect_max_x(win->frame)));
					int alpha = (darkest / max_dist) * dist;
					putpixel_alpha(screen->vmem, x, y, col, alpha);
				}
			}
		}
	}

}

void draw_window_backdrop(Screen* screen, Window* win) {
	draw_window_backdrop_segments(screen, win, 0x7);
}

Color color_rand() {
	Color c;
	c.val[0] = rand() % 255;
	c.val[1] = rand() % 255;
	c.val[2] = rand() % 255;
	return c;
}

static Window* grabbed_window = NULL;
void xserv_draw_desktop(Screen* screen) {
	static int redraw_count = 0;

	layer_clear_clip_rects(screen->vmem);
	//paint root desktop
	draw_window(screen->window);
	blit_layer(screen->vmem, screen->window->layer, screen->window->frame, screen->window->frame);

	for (int i = 0; i < screen->window->subviews->size; i++) {
		Window* win = array_m_lookup(screen->window->subviews, i);

		//we draw windows at less frequent intervals depending on how close they are to forefront
		//the foremost window is subviews[subviews.size - 1]
		//a window will get drawn only when the tick count is a multiple of their z-index
		int z_idx = i - screen->window->subviews->size;
		//reduce how often windows redraw by doubling length between redraw allowances
		z_idx *= 2;

		if (redraw_count % z_idx == 0) {
			draw_window(win);
		}

		Rect* adjusted = Rect_new(rect_min_y(win->frame),
								 rect_min_x(win->frame),
								 rect_max_y(win->frame) - 1,
								 rect_max_x(win->frame) - 1);
		layer_add_clip_context(screen->vmem, win->layer, *adjusted);
		kfree(adjusted);
		draw_window_backdrop(screen, win);
	}

	if (grabbed_window) {
		draw_window_shadow(screen, grabbed_window, grabbed_window->frame.origin);
	}

	for (int i = 0; i < screen->window->subviews->size; i++) {
		Window* win = array_m_lookup(screen->window->subviews, i);
		draw_window_backdrop(screen, win);
	}
	redraw_count++;
}

Label* fps;
static void display_about_window(Point origin) {
	//TODO this text should load off a file
	//localization?
	Window* about_win = create_window(rect_make(origin, size_make(800, 330)));
	about_win->title = "About axle";
	Label* body_label = create_label(rect_make(point_make(0, 0), size_make(about_win->content_view->frame.size.width, about_win->content_view->frame.size.height)), "Welcome to axle OS.\n\nVisit www.github.com/codyd51/axle for this OS's source code.\nYou are in axle's window manager, called xserv. xserv is a compositing window manager, meaning window bitmaps are stored offscreen and combined into a final image each frame.\n\tYou can right click anywhere on the desktop to access axle's application launcher. These are a few apps to show what axle and xserv can do, such as load a bitmap from axle's filesystem or display live CPU usage animations.\n\nIf you want to force a full xserv redraw, press 'r'\nIf you want to toggle the transparency of the topmost window between 0.5 and 1, press 'a'\n\nPress ctrl+m any time to log source file dynamic memory usage.\nPress ctrl+p at any time to log CPU usage.\n");
	body_label->font_size = size_make(8, 8);
	add_sublabel(about_win->content_view, body_label);
	present_window(about_win);
}

static Window* xterm = NULL;
void xterm_set(Window* w) {
	xterm = w;
}

Window* xterm_get() {
	return xterm;
}

static void display_xterm(Point origin) {
	Window* xterm = create_window(rect_make(origin, size_make(600, 300)));
	xterm->title = "awm IO";
	xterm->content_view->background_color = color_black();

	Label* out = create_label(rect_make(point_zero(), xterm->content_view->frame.size), "");
	out->font_size = size_make(8, 8);
	out->text_color = color_make(30, 200, 0);

	set_alpha((View*)xterm, 0.65);

	add_sublabel(xterm->content_view, out);
	present_window(xterm);
	xterm_set(xterm);
}

void desktop_setup(Screen* screen) {
	screen->window = create_window_int(rect_make(point_zero(), screen->resolution), true);
	screen->window->superview = NULL;

	//set up background image
	Bmp* background = load_bmp(screen->window->frame, "colorstone.bmp");
	if (background) {
		add_bmp(screen->window->content_view, background);
	}

	add_status_bar(screen);
	add_taskbar(screen);

	//display_sample_image(point_make(450, 100));
	//calculator_xserv(point_make(400, 300));
	//display_usage_monitor(point_make(350, 500));
	//display_about_window(point_make(100, 200));
	display_about_window(point_make(50, 100));
	display_xterm(point_make(100, 100));
}

static void draw_mouse_shadow(Screen* screen, Point old, Point new) {
	Size cursor_size = size_make(12, 14);
	for (int i = 0; i < shadow_count; i++) {
		int lerp_x = lerp(old.x, new.x, (1 / (float)shadow_count) * i);
		int lerp_y = lerp(old.y, new.y, (1 / (float)shadow_count) * i);
		Point shadow_loc = point_make(lerp_x, lerp_y);

		//draw cursor shadow
		//draw_rect(screen->vmem, rect_make(shadow_loc, cursor_size), color_make(200, 200, 230), THICKNESS_FILLED);
		//draw border
		draw_rect(screen->vmem, rect_make(shadow_loc, cursor_size), color_make(0, 150, 0), THICKNESS_FILLED);
		draw_rect(screen->vmem, rect_make(shadow_loc, cursor_size), color_black(), 1);
	}
}

static Point last_mouse_pos = { -1, -1 };
void draw_cursor(Screen* screen) {
	//actual cursor bitmap
	static Bmp* cursor = 0;
	static bool tried_loading_cursor = false;

	if (!tried_loading_cursor) {
		cursor = load_bmp(rect_make(point_zero(), size_make(12, 18)), "cursor.bmp");
		tried_loading_cursor = true;
	}

	//update cursor position
	//cursor->frame.origin = mouse_point();

	//drawing cursor shouldn't change dirtied flag
	//save dirtied flag, draw cursor, and restore it
	char prev_dirtied = dirtied;

	//we do not call add_bmp on the cursor
	//we draw it manually to ensure it is always above all other content
	if (cursor) {
		draw_bmp(screen->vmem, cursor);
	}
	else {
		//couldn't load cursor, use backup
		//draw_rect(screen->vmem, rect_make(mouse_point(), size_make(10, 12)), color_green(), THICKNESS_FILLED);
	}

	draw_mouse_shadow(screen, last_mouse_pos, mouse_point());
	last_mouse_pos = mouse_point();

	dirtied = prev_dirtied;
}

Point cursor_pos() {
	return last_mouse_pos;
}

char xserv_draw(Screen* screen) {
	dirtied = 0;
	xserv_draw_desktop(screen);
	draw_cursor(screen);

	return (char)dirtied;
}

//recursively checks view hierarchy, returning lowest view bounding point
static View* view_containing_point_sub(View* view, Point p) {
	if (rect_contains_point(view->frame, p)) {
		if (!view->subviews->size) {
			return view;
		}
		//traverse subviews and find one containing this point
		for (int i = 0; i < view->subviews->size; i++) {
			View* subview = (View*)array_m_lookup(view->subviews, i);

			//convert point to subview's coordinate space
			Point converted = p;
			converted.x -= view->frame.origin.x;
			converted.y -= view->frame.origin.y;

			if (rect_contains_point(subview->frame, converted)) {
				return view_containing_point_sub(subview, converted);
			}
		}
		//no subviews contained the point
		//return root window
		return view;
	}
	//this view doesn't bound the point
	return NULL;
}

//uses view_containing_point_sub to try to find a point owner
//if none own click, checks against window's title and content views
static View* view_containing_point(Window* window, Point p) {
	//is this point bounded at all?
	if (!rect_contains_point(window->frame, p)) {
		return NULL;
	}

	p.x -= window->frame.origin.x;
	p.y -= window->frame.origin.y;

	//quick check to check if it was in the window's title view
	if (rect_contains_point(window->title_view->frame, p)) {
		return window->title_view;
	}

	//remove title view offset
	Point c = p;
	c.x -= window->title_view->frame.origin.x;
	c.y -= window->title_view->frame.origin.y;

	View* owner = view_containing_point_sub(window->content_view, c);
	if (owner) {
		return owner;
	}

	return window->content_view;
}

static Window* window_containing_point(Point p) {
	Screen* screen = gfx_screen();
	//traverse window hierarchy, starting with the topmost window
	for (int i = screen->window->subviews->size - 1; i >= 0; i--) {
		Window* w = (Window*)array_m_lookup(screen->window->subviews, i);
		if (rect_contains_point(w->frame, p)) {
			return w;
		}
	}
	//wasn't in any subwindows
	//point must be within root window
	return screen->window;
}

static void set_active_window(Screen* screen, Window* grabbed_window) {
	active_window = grabbed_window;
	for (int i = 0; i < screen->window->subviews->size; i++) {
		Window* win = array_m_lookup(screen->window->subviews, i);
		Color color;
		if (win == active_window) {
			color = color_make(120, 245, 80);
		}
		else {
			color = color_make(50, 122, 40);
		}
		set_background_color(win->title_view, color);
		mark_needs_redraw((View*)win);
	}
	printk("setting PID %d to first responder...\n", grabbed_window->owner_pid);
	become_first_responder_pid(grabbed_window->owner_pid);
}

static Point world_point_to_owner_space_sub(Point p, View* view) {
	if (rect_contains_point(view->frame, p)) {
		p.x -= view->frame.origin.x;
		p.y -= view->frame.origin.y;

		if (!view->subviews->size) {
			return p;
		}
		//traverse subviews
		for (int i = 0; i < view->subviews->size; i++) {
			View* subview = (View*)array_m_lookup(view->subviews, i);
			if (rect_contains_point(subview->frame, p)) {
				return world_point_to_owner_space_sub(p, subview);
			}
		}
	}
	return p;
}

//converts gloabl point to view's coordinate space
Point world_point_to_owner_space(Point p) {
	Point converted = p;
	Window* win = window_containing_point(p);
	converted.x -= win->frame.origin.x;
	converted.y -= win->frame.origin.y;

	if (rect_contains_point(win->title_view->frame, converted)) {
		return converted;
	}

	return world_point_to_owner_space_sub(converted, win->content_view);
}

void process_kb_events(Screen* screen) {
	//check if there are any keys pending
	if (!haskey()) return;

	char ch;
	if (read(0, &ch, 1)) {
		if (ch == 'q') {
			//quit xserv
			xserv_quit(screen);
		}
		else if (ch == 'r') {
			//force everything to refresh
			screen->window->needs_redraw = 1;
			for (int i = 0; i < screen->window->subviews->size; i++) {
				Window* w = array_m_lookup(screen->window->subviews, i);
				w->needs_redraw = 1;
			}
		}
		else if (ch == 'a') {
			//toggle alpha of topmost window between 0.5 and 1.0
			if (screen->window->subviews->size) {
				Window* topmost = array_m_lookup(screen->window->subviews, screen->window->subviews->size - 1);
				float new = 0.5;
				if (topmost->layer->alpha == new) {
					new = 1.0;
				}
				set_alpha((View*)topmost, new);
			}
		}
		else if (ch == 'c') {
			//calculator_xserv(point_make(100, 500));
		}
	}
}

static void process_mouse_events(Screen* screen) {
	static uint8_t last_event = 0;

	//get mouse events
	uint8_t events = mouse_events();
	Point p = mouse_point();

	//0th bit is left mouse button
	bool left = events & 0x1;
	//2nd bit is right button
	bool right = events & 0x2;

	//find the window that got this click
	Window* owner = window_containing_point(p);
	//find element within window that owns click
	View* local_owner = view_containing_point(owner, p);

	if (left) {
		if (!grabbed_window) {
			//don't move root window! :p
			if (owner != screen->window && owner->layer->alpha > 0.0) {
				set_active_window(screen, owner);

				//bring this window to forefont
				array_m_remove(screen->window->subviews, array_m_index(screen->window->subviews, (type_t)active_window));
				array_m_insert(screen->window->subviews, (type_t)active_window);

				//only move window if title view was selected
				if (local_owner == owner->title_view) {
					grabbed_window = owner;
					last_grabbed_window_pos = grabbed_window->frame.origin;
				}
			}
		}
		else {
			if (last_mouse_pos.x != -1 && grabbed_window != screen->window) {
				//move this window by the difference between current mouse position and last mouse position
				Rect old_frame = grabbed_window->frame;
				Rect new_frame = old_frame;
				new_frame.origin.x -= (last_mouse_pos.x - p.x);
				new_frame.origin.y -= (last_mouse_pos.y - p.y);

				set_frame((View*)grabbed_window, new_frame);
				last_grabbed_window_pos = old_frame.origin;
			}
		}
	}
	else {
		if (grabbed_window) {
			//click event ended, release window
			grabbed_window = NULL;
		}
	}

	if (local_owner) {
		printf("mouse event in local owner 0x%08x, buttons size %d\n", local_owner, local_owner->buttons->size);
		for (int j = 0; j < local_owner->buttons->size; j++) {
			//convert point to local coordinate space
			Point conv = world_point_to_owner_space(p);

			Button* b = (Button*)array_m_lookup(local_owner->buttons, j);
			if (rect_contains_point(b->frame, conv)) {
				printf("rect contains point");
				//only perform mousedown handler if mouse was previously not clicked
				if (left && !(last_event & 0x1)) {
					button_handle_mousedown(b);
				}
				//only perform mouseup handler if mouse was just released
				else if (!left && (last_event & 0x1)) {
					button_handle_mouseup(b);
				}
				break;
			}
			else {
				printf("rect not in ");
			}
		}
	}

	//did a right click just happen?
	if (right && !(last_event & 0x2)) {
		printk("invoking launcher for right click...\n");
		//launcher_invoke(p);
	}

	draw_mouse_shadow(screen, last_mouse_pos, p);

	last_mouse_pos = p;
	last_event = events;
}

void xserv_refresh(Screen* screen) {
	//if (!screen->finished_drawing) return;

	long time_start = time();
	static long last_redraw = 0;

	//handle mouse events
	process_mouse_events(screen);
	//keyboard events
	//process_kb_events(screen);
	//main refresh loop
	//traverse view hierarchy,
	//redraw views if necessary,
	//composite everything onto root layer
	xserv_draw(screen);

	long frame_end = time();

	update_all_animations(screen, frame_end - time_start);

	frame_end = time();
	long frame_time = (frame_end - time_start);

	long render_time = frame_time;
	long real_fps = 1000 / (frame_end - last_redraw);

	char buf[32];
	strcpy(buf, " real (fps): ");
	itoa(real_fps, &(buf[strlen(buf)]));
	strcat(buf, "\nrender (ms): ");
	char* next = &(buf[strlen(buf)]);
	itoa(render_time, next);

	set_text(fps, buf);
	draw_label(screen->window->layer, fps);

	/*
	write_screen_region(fps->frame);
	Rect new_cursor_rect = rect_make(cursor_pos(), size_make(12, 14));
	Rect cursor_bound_rect = rect_union(old_cursor_rect, new_cursor_rect);
	cursor_bound_rect = rect_inset(cursor_bound_rect, 12, 14);
	write_screen_region(cursor_bound_rect);

	write_screen_region(modified_viewport);
	*/
	write_screen(screen);

	last_redraw = time_start;

	dirtied = 0;
}

void rect_print(Rect r) {
	printk("{{%d,%d},{%d,%d}}\n", rect_min_x(r),
								  rect_min_y(r),
								  rect_max_x(r),
								  rect_max_y(r));
}

void xserv_pause() {
	//switch_to_text();
}

void xserv_resume() {
	//switch_to_vesa(0x118, false);
}

void xserv_temp_stop(uint32_t pause_length) {
	xserv_pause();
	sleep(pause_length);
	xserv_resume();
}

void xserv_init() {
	become_first_responder();
	Screen* screen = gfx_screen();
	printf("screen 0x%08x vmem 0x%08x\n", screen, screen->vmem);
	desktop_setup(screen);

	//add FPS tracker
	//don't call add_sublabel on fps because it's drawn manually
	//(drawn manually so we can update text with accurate frame draw time)
	fps = create_label(rect_make(point_make(5, 10), size_make(150, 30)), "FPS counter");
	fps->text_color = color_black();

	//test_xserv();
	/*
	if (!sys_fork()) {
		char* argv[] = {"ash", NULL};
		execve(argv[0], argv, NULL);
		ASSERT(0, "execve returned");
	}
	/*
	if (!sys_fork()) {
		char* argv[] = {"files", NULL};
		become_first_responder_pid(getpid());
		execve(argv[0], argv, NULL);
		ASSERT(0, "execve returned");
	}
	*/

	while (1) {
		xserv_refresh(screen);
		//sys_yield(RUNNABLE);
		//mouse_event_wait(); 
	}

	_kill();
}

void xserv_fail() {
	printk_err("xserv fail");
	Screen* screen = gfx_screen();
	if (!screen) return;

	float rect_width = rect_max_x(screen->window->frame);
	float rect_height = rect_max_y(screen->window->frame) / 8;
	float origin_x = 0;
	float origin_y = (rect_max_y(screen->window->frame) / 2) - (rect_height / 2);
	Rect error_box = rect_make(point_make(origin_x, origin_y), size_make(rect_width, rect_height));

	draw_rect(screen->vmem, error_box, color_red(), THICKNESS_FILLED);
	draw_rect(screen->vmem, error_box, color_black(), 2);
	draw_string(screen->vmem, "CRITICAL ERROR: xserv has died.\naxle will switch back to text in 5 seconds.\nIf this fails, please restart axle.\nError: corrupted heap", point_make(error_box.origin.x + CHAR_WIDTH, error_box.origin.y + CHAR_HEIGHT), color_black(), size_make(CHAR_WIDTH, CHAR_HEIGHT));
	write_screen(screen);
	sleep(5000);
	kernel_end_critical();
}

char* xserv_win_create(Window** UNUSED(out), Rect* UNUSED(frame)) {
	//*out = task_register_window(*frame);
	/*
	char* test = kmalloc_a(PAGE_SIZE);
	strcpy(test, "Message sent from kernel");
	char* destination = NULL;
	ipc_send(test, PAGE_SIZE, getpid(), &destination);
	printf("xserv_win_create dest %x\n", destination);
	return destination;
	*/
	return 0;
}

void xserv_win_present(Window* win) {
	present_window(win);
}

void xserv_win_destroy(Window* win) {
	window_teardown(win);
}
