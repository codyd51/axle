#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libutils/assert.h>
#include <libagx/lib/shapes.h>
#include <libgui/libgui.h>
#include <libimg/libimg.h>
#include <libamc/libamc.h>
#include <file_manager/file_manager_messages.h>

#include "image_viewer_messages.h"

typedef struct raw_image_info {
	const char* image_name;
	image_t* image;
} raw_image_info_t;

static raw_image_info_t* _g_image = NULL;
static gui_window_t* _g_window = NULL;

static Rect _image_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static void _render_image(gui_view_t* view) {
	if (!_g_image) {
		return;
	}
	image_render_to_layer(
		_g_image->image,
		view->content_layer->fixed_layer.inner,
		rect_make(
			point_zero(),
			view->content_layer_frame.size
		)
	);
}

static Rect _window_resized(gui_view_t* view, Size window_size) {
	_render_image(view);
	return rect_zero();
}

static void _amc_message_received(amc_message_t* msg) {
    const char* source_service = msg->source;

	image_viewer_load_image_request_t* load_image_req = (image_viewer_load_image_request_t*)&msg->body;
	assert(load_image_req->event == IMAGE_VIEWER_LOAD_IMAGE, "Expected load image message");

	file_manager_read_file_request_t req = {0};
	req.event = FILE_MANAGER_READ_FILE;
	snprintf(req.path, sizeof(req.path), "%s", load_image_req->path);
	amc_message_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(file_manager_read_file_request_t));

	printf("Requested file %s\n", req.path);
	amc_message_t* file_data_msg;
	printf("Awaiting response...\n");
	amc_message_await__u32_event(FILE_MANAGER_SERVICE_NAME, FILE_MANAGER_READ_FILE_RESPONSE, &file_data_msg);
	printf("Got response...\n");
	file_manager_read_file_response_t* resp = (file_manager_read_file_response_t*)&file_data_msg->body;
	uint8_t* b = &resp->file_data;

	if (_g_image != NULL) {
		printf("Freeing previous image...\n");
		image_free(_g_image->image);
		free(_g_image->image_name);
		free(_g_image);
	}

	raw_image_info_t* raw_image = calloc(1, sizeof(raw_image_info_t));
	raw_image->image_name = strdup(load_image_req->path);

	raw_image->image = image_parse(resp->file_size, resp->file_data);

	_g_image = raw_image;
	_render_image(array_lookup(_g_window->views, 0));
}

int main(int argc, char** argv) {
	amc_register_service(IMAGE_VIEWER_SERVICE_NAME);

	_g_window = gui_window_create("Image Viewer", 400, 400);
	Size window_size = _g_window->size;

	gui_view_t* image_view = gui_view_create(
		_g_window,
		(gui_window_resized_cb_t)_image_view_sizer
	);
	image_view->controls_content_layer = true;
	image_view->window_resized_cb = (gui_window_resized_cb_t)_window_resized;

	gui_add_message_handler(_amc_message_received);
	gui_enter_event_loop();

	return 0;
}
