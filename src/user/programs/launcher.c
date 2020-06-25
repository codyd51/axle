#include "launcher.h"
#include <gfx/lib/gfx.h>
#include <gfx/lib/window.h>
#include <user/programs/calculator.h>
#include <gfx/lib/button.h>
#include <std/string.h>

void display_sample_image(Point origin) {
		Window* image_viewer = create_window(rect_make(origin, size_make(512, 512)));
		image_viewer->title = "Image Viewer";
		image_viewer->content_view->background_color = color_make(135, 206, 250);
		Bmp* bmp = load_bmp(rect_make(point_zero(), size_make(512, 512)), "pillar.bmp");
		if (bmp) {
			add_bmp(image_viewer->content_view, bmp);
		}
		present_window(image_viewer);
}

static void launcher_buttonpress(Button* b) {
	char* title = b->label->text;
	if (!strcmp(title, "Image Viewer")) {
		display_sample_image(point_make(400, 300));
	}
	if (!strcmp(title, "Text Viewer")) {
		Window* label_win = create_window(rect_make(point_make(100, 100), size_make(500, 500)));
		label_win->title = "Text test";
		Label* test_label = create_label(rect_make(point_make(10, 10), size_make(label_win->content_view->frame.size.width - 10, label_win->content_view->frame.size.height - 20)), "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque pulvinar dui bibendum nunc convallis, bibendum venenatis mauris ornare. Donec et libero lacus. Nulla tristique auctor pulvinar. Aenean enim elit, malesuada nec dignissim eget, varius ac nunc. Vestibulum varius lectus nisi, in dignissim orci volutpat in. Aliquam eget eros lorem. Quisque tempor est a rhoncus consequat. Quisque vestibulum finibus sapien. Etiam enim sem, vehicula ac lorem vitae, mattis mollis mauris. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Vivamus eleifend dui vel nulla suscipit pretium. Suspendisse vel nunc efficitur, lobortis dui convallis, tristique tellus. Ut ut viverra est. Etiam tempor justo risus. Cras laoreet eu sapien et lacinia. Nunc imperdiet blandit purus a semper.");
		add_sublabel(label_win->content_view, test_label);
		present_window(label_win);
	}
	if (!strcmp(title, "Calculator")) {
		calculator_xserv(point_make(100, 500));
	}

	launcher_dismiss();
}

static Window* launcher_win = NULL;

void launcher_teardown() {
	launcher_win = NULL;
}

void launcher_invoke(Point origin) {
	if (launcher_win) return;

	Size button_size = size_make(150, 50);
	int num_buttons = 3;
	launcher_win = create_window(rect_make(origin, size_make(button_size.width, WINDOW_TITLE_VIEW_HEIGHT + (button_size.height * num_buttons))));
	launcher_win->teardown_handler = &launcher_teardown;
	launcher_win->title = "Launcher";

	for (int i = 0; i < num_buttons; i++) {
		char* title;
		switch (i) {
			case 0:
				title = "Image Viewer";
				break;
			case 1:
				title = "Text Viewer";
				break;
			case 2:
			default:
				title = "Calculator";
				break;
		}

		Button* b = create_button(rect_make(point_make(0, button_size.height * i), button_size), title);
		b->mousedown_handler = (event_handler)&launcher_buttonpress;
		b->mouseup_handler = (event_handler)&launcher_dismiss;
		add_button(launcher_win->content_view, b);
	}
	
	present_window(launcher_win);
}

void launcher_dismiss() {
	if (!launcher_win) return;

	kill_window(launcher_win);
	launcher_teardown();
}

