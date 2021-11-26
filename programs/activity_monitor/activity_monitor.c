#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <libgui/libgui.h>

static gui_text_view_t* _g_text_view = NULL;

static Rect _logs_text_view_sizer(gui_text_view_t* logs_text_view, Size window_size) {
	return rect_make(point_zero(), window_size);
}

static void _amc_message_received(gui_window_t* window, amc_message_t* msg) {
	char buf[msg->len+1];
	strncpy(buf, msg->body, msg->len);
	buf[msg->len] = '\0';
	gui_text_view_puts(_g_text_view, buf, color_white());
}

static void _refresh_stats(gui_text_view_t* view) {
	printf("Refreshing stats\n");

	uint32_t req = AMC_SYSTEM_PROFILE_REQUEST;
	amc_message_send(AXLE_CORE_SERVICE_NAME, &req, sizeof(req));

	amc_message_t* msg;
	amc_message_await(AXLE_CORE_SERVICE_NAME, &msg);
	uint32_t event = amc_msg_u32_get_word(msg, 0);
	assert(event == AMC_SYSTEM_PROFILE_RESPONSE, "blah");
	amc_system_profile_response_t* info = (amc_system_profile_response_t*)msg->body;

	char buf[128];
	snprintf(buf, sizeof(buf), "- Snapshot at %dms -\n", ms_since_boot());
	gui_text_view_puts(view, buf, color_white());

	snprintf(buf, sizeof(buf), "Allocated RAM: %.2fmb\n", info->pmm_allocated / (float)(1024 * 1024));
	gui_text_view_puts(view, buf, color_white());

	snprintf(buf, sizeof(buf), "Kernel heap: %.2fmb\n", info->kheap_allocated / (float)(1024 * 1024));
	gui_text_view_puts(view, buf, color_white());
	gui_text_view_puts(view, "-                    -\n", color_white());

	gui_timer_start(3000, (gui_timer_cb_t)_refresh_stats, view);
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.activity_monitor");

	gui_window_t* window = gui_window_create("Activity Monitor", 500, 200);
	_g_text_view = gui_text_view_create(
		window,
		(gui_window_resized_cb_t)_logs_text_view_sizer
	);
	_g_text_view->font_size = size_make(16, 24);

	gui_timer_start(0, (gui_timer_cb_t)_refresh_stats, _g_text_view);
	gui_enter_event_loop();

	return 0;
}
