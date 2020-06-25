#include "usage_monitor.h"
#include <gfx/lib/gfx.h>
#include <kernel/multitasking/tasks/record.h>
#include <std/math.h>
#include <gfx/lib/shapes.h>

//TODO instead of being global, this should be context to teardown func
Window* usage_win = 0;
void usage_monitor_teardown() {
	if (usage_win) {
		window_teardown(usage_win);
	}
	_kill();
}

void update_usage_stats(Window* win) {
	draw_rect(win->content_view->layer, rect_make(point_zero(), win->content_view->layer->size), color_white(), THICKNESS_FILLED);
	task_history_t* history = sched_get_task_history();

	//find longest task name
	int longest_len = 0;
	for (int i = 0; i < history->count; i++) {
		int curr_len = strlen(history->history[i]);
		longest_len = MAX(curr_len, longest_len);
	}
	longest_len += 4;


	int history_count = 5;
	float section_height = win->frame.size.height / ((float)history_count + 2.0);
	//int label_length = (longest_len + 12) * CHAR_WIDTH;
	int label_length = 12 * CHAR_WIDTH;

	for (int i = 0; i < history_count; i++) {
		//print name, followed by required % of spaces to align output
		float total_width = win->frame.size.width - label_length;
		float percent_cpu = (history->vals[i] / (float)history->time);
		//float percent_cpu = (rand() % 100) / 100.0;

		if (i == history->count) {
			static float last_test_bar_percent = 0.0;
			percent_cpu = last_test_bar_percent;
			last_test_bar_percent += 0.01;
			if (last_test_bar_percent > 1.0) {
				last_test_bar_percent = 0;
			}
		}

		int width_from_percent = total_width * (percent_cpu + 0.025);

		char name[64];

		//name of test bar
		if (true || i == history->count) {
			strcpy((char*)&name, "bar demo");
		}
		else {
			sprintf((char*)&name, "%s (%f CPU)", history->history[i], percent_cpu * 100);
		}

		int diff = longest_len - strlen(name);
		for (int j = 0; j < diff; j++) {
			strcat((char*)&name, " ");
		}
		draw_string(win->content_view->layer, (char*)&name, point_make(CHAR_WIDTH, CHAR_HEIGHT + (section_height * i)), color_black(), size_make(CHAR_WIDTH, CHAR_HEIGHT));

		draw_line(win->content_view->layer, line_make(point_make(0, section_height * (i+1)), point_make(label_length, section_height * (i+1))), color_gray(), 1);
		draw_line(win->content_view->layer, line_make(point_make(label_length, section_height * (i+1)), point_make(win->content_view->frame.size.width, section_height * (i + 1))), color_black(), 1);

		//green-orange-red bar gradient
		float gradient_percent = percent_cpu * 2;
		Color from = color_make(0, 190, 20);
		Color to = color_orange();

		if (gradient_percent >= 1.0) {
			from = color_orange();
			to = color_red();
			gradient_percent -= 1.0;
		}

		Color bar_color = color_make(lerp(from.val[0], to.val[0], gradient_percent),
									 lerp(from.val[1], to.val[1], gradient_percent),
									 lerp(from.val[2], to.val[2], gradient_percent));

		Rect bar = rect_make(point_make(label_length, (section_height * i) + (section_height * 0.25)), size_make(width_from_percent, section_height * 0.5));
		draw_rect(win->content_view->layer, bar, bar_color, THICKNESS_FILLED);
	}

	draw_line(win->content_view->layer, line_make(point_make(label_length, 0), point_make(label_length, win->content_view->frame.size.height)), color_black(), 1);

	kfree(history);
}

void display_usage_monitor(Point origin) {
	Window* usage_win = create_window(rect_make(origin, size_make(500, 260)));
	usage_win->teardown_handler = usage_monitor_teardown;
	usage_win->title = "Usage monitor";
	usage_win->redraw_handler = (event_handler)update_usage_stats;
	present_window(usage_win);
}
