#include "usage_monitor.h"
#include <gfx/lib/gfx.h>
#include <kernel/util/multitasking/tasks/record.h>
#include <std/math.h>
#include <gfx/lib/shapes.h>

//TODO instead of being global, this should be context to teardown func
Window* usage_win = 0;
static void usage_monitor_teardown() {
	if (usage_win) {
		window_teardown(usage_win);
	}
	_kill();
}

void update_usage_stats(Window* win) {
	task_history_t* history = sched_get_task_history();

	//find longest task name
	int longest_len = 0;
	for (int i = 0; i < history->count; i++) {
		char* curr = history->history[i];
		int curr_len = strlen(history->history[i]);
		longest_len = MAX(curr_len, longest_len);
	}

	float section_height = win->frame.size.height / ((float)history->count + 1.0);
	int label_length = (longest_len + 15) * CHAR_WIDTH;

	draw_line(win->content_view->layer, line_make(point_make(label_length, 0), point_make(label_length, win->content_view->frame.size.height)), color_black(), 1);

	for (int i = 0; i < history->count; i++) {
		//print name, followed by required % of spaces to align output
		float total_width = win->frame.size.width - label_length;
		float percent_cpu = (history->vals[i] / (float)history->time);
		int width_from_percent = total_width * percent_cpu;

		char name[64];
		sprintf((char*)&name, "%s (%f CPU)", history->history[i], percent_cpu * 100);
		int diff = longest_len - strlen(name);
		for (int j = 0; j < diff; j++) {
			strcat(&name, " ");
		}
		draw_string(win->content_view->layer, &name, point_make(CHAR_WIDTH, CHAR_HEIGHT + (section_height * i)), color_black());

		draw_line(win->content_view->layer, line_make(point_make(label_length, section_height * (i+1)), point_make(win->content_view->frame.size.width, section_height * (i + 1))), color_black(), 1);

		Rect bar = rect_make(point_make(label_length, (section_height * i) + (section_height * 0.25)), size_make(width_from_percent, section_height * 0.5));
		draw_rect(win->content_view->layer, bar, color_blue(), THICKNESS_FILLED);
	}
	kfree(history);
}

void usage_monitor_launch() {
	if (fork("monitor")) {
		return;
	}
	sleep(3000);

	/*
	Window* usage_win = create_window(rect_make(point_zero(), size_make(400, 400)));
	usage_win->teardown_handler = usage_monitor_teardown;
	usage_win->title = "Usage monitor";
	present_window(usage_win);
	*/

	while (1) {
		update_usage_stats(usage_win);
	}
}

