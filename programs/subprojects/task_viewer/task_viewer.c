#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include <libgui/libgui.h>

#define TASK_VIEWER_GET_TASK_INFO 777
typedef struct task_viewer_get_task_info {
	uint32_t event;	// TASK_VIEWER_GET_TASK_INFO
} task_viewer_get_task_info_t;

typedef struct vas_range {
	uint64_t start;
	uint64_t size;
} vas_range_t;

typedef struct task_info {
    char name[64];
	uint32_t pid;
    uint64_t rip;
    uint64_t vas_range_count;
    vas_range_t vas_ranges[16];
    uint64_t user_mode_rip;
	bool has_amc_service;
	uint64_t pending_amc_messages;
} task_info_t;

typedef struct task_viewer_get_task_info_response {
	uint32_t event; // TASK_VIEWER_GET_TASK_INFO
    uint32_t task_info_count;
    task_info_t tasks[];
} task_viewer_get_task_info_response_t;

#define MAX_TASK_COUNT 64

typedef struct state {
	gui_window_t* window;
	gui_scroll_view_t* scroll_view;
	int task_count;
	task_info_t tasks[MAX_TASK_COUNT];
} state_t;

state_t _g_state = {0};

Rect _main_view_sizer(gui_view_t* v, Size superview_size) {
	return rect_make(point_zero(), superview_size);
}

typedef struct view_with_info {
	gui_view_t view;
	int idx;
	uint64_t rip;
} view_with_info_t;

#define TASK_HEIGHT 126

static void _scroll_view_sizer(view_with_info_t* view, Size superview_size) {
	return rect_make(point_zero(), superview_size);
}

static void _task_view_sizer(view_with_info_t* view, Size superview_size) {
	gui_scroll_view_t* superview = (gui_scroll_view_t*)view->view.superview;
	//printf("Got superview 0x%016lx %ld\n", superview, superview->full_content_area_size.width);

	int height = TASK_HEIGHT;
	int origin_y = height * view->idx;
	return rect_make(
		point_make(
			0,
			origin_y
		),
		size_make(
			//superview->full_content_area_size.width,
			//////superview_size.width,
			superview->base.content_layer_frame.size.width,
			height
		)
	);
}

/*
static void _layout_tasks() {
	// Remove all subviews from the main content view
	int view_count = _g_state.window->views->size;
	for (int i = view_count; i > 0; i--) {
		int view_idx = i - 1;
		gui_view_t* view = array_lookup(_g_state.window->views, view_idx);
		gui_view_remove_from_superview_and_destroy(&view);
		//printf("View after remove: 0x%016lx\n", view);
	}

	// Lay out new views
	for (int i = 0; i < _g_state.task_count; i++) {
		task_info_t* task = &_g_state.tasks[i];
		view_with_info_t* view = calloc(1, sizeof(view_with_info_t));
		gui_view_alloc_dynamic_fields((gui_view_t*)view);
		gui_view_init((gui_view_t*)view, _g_state.window, (gui_window_resized_cb_t)_task_view_sizer);
		gui_view_add_to_window(view, _g_state.window);

		char buf[512];
		snprintf(buf, sizeof(buf), "Task %d: %s, RIP 0x%016lx", i, task->name, task->rip);
		gui_view_set_title((gui_view_t*)view, buf);
	}
}
*/

void _format_size(uint64_t size, char* buf, uint64_t buf_size) {
	if (size < 1024) {
		snprintf(buf, buf_size, "%d", size);
	}
	else if (size < 1024 * 1024) {
		snprintf(buf, buf_size, "%dkb", size / 1024);
	}
	else if (size < 1024 * 1024 * 1024) {
		snprintf(buf, buf_size, "%dmb", size / 1024 / 1024);
	}
	else {
		snprintf(buf, buf_size, "%dgb", size / 1024 / 1024 / 1024);
	}
}

static Rect _draw_string(gui_view_t* view, char* text, Point origin, Size font_size, Color text_color, Color background_color, int extra_width, int extra_height) {
	Point margin = point_make(2, 2);
	Rect background = rect_make(
		origin,
		size_make(
			font_size.width * strlen(text) + (margin.x * 2) + extra_width,
			font_size.height + (margin.y * 2) + extra_height
		)
	);
	gui_layer_draw_rect(view->content_layer, background, background_color, THICKNESS_FILLED);
	Point cursor = point_make(origin.x + margin.x + (extra_width / 2), origin.y + margin.y + (extra_height / 2));
	for (uint32_t i = 0; i < strlen(text); i++) {
		gui_layer_draw_char(
			view->content_layer,
			text[i],
			cursor.x,
			cursor.y,
			text_color,
			font_size
		);
		cursor.x += font_size.width;
	}
	
	return rect_make(
		// Don't include the Y margin in the returned cursor
		point_make(rect_max_x(background), origin.y),
		background.size
	);
}

static void _layout_tasks() {
	// Remove all subviews from the main scroll view
	int view_count = _g_state.scroll_view->base.subviews->size;
	//printf("View count %d\n", view_count);
	for (int i = view_count; i > 0; i--) {
		int view_idx = i - 1;
		//printf("Removing view %d\n", view_idx);
		gui_view_t* view = array_lookup(_g_state.scroll_view->base.subviews, view_idx);
		gui_view_remove_from_superview_and_destroy(&view);
		//printf("View after remove: 0x%016lx\n", view);
	}

	// Lay out new views
	for (int i = 0; i < _g_state.task_count; i++) {
		task_info_t* task = &_g_state.tasks[i];
		view_with_info_t* view = calloc(1, sizeof(view_with_info_t));

		// It's important that this is set before the sizer runs, which is kicked off by the libgui code below
		// Otherwise, this will be shown in an incorrect index
		// Also, order them bottom-to-top so new tasks show up first
		view->idx = _g_state.task_count - i - 1;
		view->rip = task->rip;

		gui_view_alloc_dynamic_fields((gui_view_t*)view);
		gui_view_init((gui_view_t*)view, _g_state.window, (gui_window_resized_cb_t)_task_view_sizer);
		gui_view_add_subview(&_g_state.scroll_view->base, view);

		//printf("Laying out new view %d\n", i);
		//gui_view_set_title((gui_view_t*)view, buf);
		view->view.controls_content_layer = true;

		gui_layer_draw_rect(
			view->view.content_layer, 
			rect_make(point_zero(), view->view.content_layer_frame.size),
			color_white(),
			THICKNESS_FILLED
		);

		//char buf[512];
		//snprintf(buf, sizeof(buf), "Task %d: %s, RIP 0x%016lx", i, task->name, task->rip);

		Size font_size = size_make(8, 12);
		Point cursor = point_make(4, 4);
		char name_buf[512];
		snprintf(name_buf, sizeof(name_buf), "PID %02d %s", task->pid, task->name);
		_draw_string(&view->view, name_buf, cursor, font_size, color_black(), color_white(), 0, 0);

		// Draw user-mode RIP, if available, else draw a blank space
		cursor = point_make(4, cursor.y + (font_size.height * 2));
		char buf[512] = {0};
		if (task->user_mode_rip != 0) {
			snprintf(buf, sizeof(buf), "  PC  0x%016lx", task->user_mode_rip);
		}
		_draw_string(&view->view, buf, cursor, font_size, color_black(), color_white(), 0, 0);

		// Draw AMC service info, if available, else draw a blank space
		cursor = point_make(4, cursor.y + (font_size.height * 1.5));
		memset(buf, 0, sizeof(buf));
		if (task->has_amc_service) {
			snprintf(buf, sizeof(buf), "  Pending messages: %d", task->pending_amc_messages);
		}
		_draw_string(&view->view, buf, cursor, font_size, color_black(), color_white(), 0, 0);

		cursor = point_make(4, cursor.y + (font_size.height * 2));
		for (int j = 0; j < task->vas_range_count; j++) {
			vas_range_t* range = &task->vas_ranges[j];
			char buf[128];
			_format_size(range->size, &buf, sizeof(buf));
			//printf("Size: %s, %.02f\n", buf, log2(range->size));
			int extra_width = (log2(range->size) - 12) * 4;
			Rect string_end = _draw_string(
				&view->view, 
				buf, 
				cursor, 
				font_size, 
				color_black(), 
				color_make(200, 200, 200),
				extra_width,
				4
			);

			int px_to_next_range = 16 + (log2(range->start) - 22);
			//printf("Log2 start 0x%016lx %.2f\n", range->start, log2(range->start));
			if (j < task->vas_range_count - 1) {
				// Draw a line connecting this VAS range to the next one
				int midpoint_y = string_end.origin.y + (string_end.size.height / 2);
				gui_layer_draw_line(
					view->view.content_layer, 
					line_make(
						point_make(
							string_end.origin.x,
							midpoint_y
						),
						point_make(
							string_end.origin.x + px_to_next_range,
							midpoint_y
						)
					),
					color_black(),
					4
				);
			}

			cursor = point_make(
				string_end.origin.x + px_to_next_range,
				string_end.origin.y
			);
		}
	}
}

static void _refresh_tasks(void) {
	printf("Refreshing tasks from kernel\n");
	task_viewer_get_task_info_t request = {
		.event = TASK_VIEWER_GET_TASK_INFO,
	};
	amc_message_send(AXLE_CORE_SERVICE_NAME, &request, sizeof(task_viewer_get_task_info_t));
	amc_message_t* response_msg;
	amc_message_await(AXLE_CORE_SERVICE_NAME, &response_msg);
	task_viewer_get_task_info_response_t* response = (task_viewer_get_task_info_response_t*)response_msg->body;
	assert(response->event == TASK_VIEWER_GET_TASK_INFO);

	_g_state.task_count = response->task_info_count;
	for (int i = 0; i < response->task_info_count; i++) {
		task_info_t* task = &response->tasks[i];
		memcpy(&_g_state.tasks[i], task, sizeof(task_info_t));
	}

	_layout_tasks();

	// Schedule another refresh
	gui_timer_start(4000, _refresh_tasks, NULL);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.task_viewer");

	_g_state.window = gui_window_create("Virtual Address Space Visualizer", 740, 600);
	_g_state.scroll_view = gui_scroll_view_create(_g_state.window, (gui_window_resized_cb_t)_scroll_view_sizer);

	_refresh_tasks();

	gui_enter_event_loop();

	return 0;
}
