#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

// Layers and drawing
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>
#include <libgui/libgui.h>

// Window management
#include <awm/awm.h>

// Communication with other processes
#include <libamc/libamc.h>

#include <libgui/gui_text_input.h>
#include <libgui/gui_view.h>
#include <libgui/gui_slider.h>

#include "preferences_messages.h"

typedef struct state {
    gui_slider_t* from_red;
    gui_slider_t* from_green;
    gui_slider_t* from_blue;
    gui_slider_t* to_red;
    gui_slider_t* to_green;
    gui_slider_t* to_blue;
    gui_view_t* from_sliders_container;
    gui_view_t* to_sliders_container;
    gui_view_t* preview;
    gui_view_t* apply_view;
    gui_button_t* apply_button;
} state_t;

static state_t _g_state = {0};

static Rect _preview_sizer(gui_view_t* v, Size window_size) {
    uint32_t t = window_size.width * 0.25;
    return rect_make(
        point_make(
            window_size.width - t, 0
        ),
        size_make(t, window_size.height / 2)
    );
}

static Rect _apply_view_sizer(gui_view_t* v, Size window_size) {
    uint32_t t = window_size.width * 0.25;
    return rect_make(
        point_make(
            window_size.width - t, window_size.height / 2
        ),
        size_make(t, window_size.height / 2)
    );
}

static Rect _from_sliders_container_sizer(gui_view_t* v, Size window_size) {
    uint32_t t = window_size.width * 0.75;
    uint32_t h = window_size.height / 2;
    return rect_make(
        point_make(
            0,
            0
        ),
        size_make(
            t,
            h
        )
    );
}

static Rect _to_sliders_container_sizer(gui_view_t* v, Size window_size) {
    uint32_t t = window_size.width * 0.75;
    uint32_t h = window_size.height / 2;
    return rect_make(
        point_make(
            0,
            h
        ),
        size_make(
            t,
            h
        )
    );
}

static Rect _red_slider_sizer(gui_slider_t* s, Size window_size) {
    Rect parent_frame = s->superview->content_layer_frame;
    uint32_t usable_height = (parent_frame.size.height / 6) * 5;
    uint32_t width = parent_frame.size.width * 0.8;
    uint32_t height = usable_height / 6;
    uint32_t mid_y = parent_frame.size.height / 2;
    return rect_make(
        point_make(
            (parent_frame.size.width / 2) - (width / 2),
            mid_y - (height / 2.0) - (height * 2)
        ),
        size_make(
            width,
            height
        )
    );
}

static Rect _green_slider_sizer(gui_slider_t* s, Size window_size) {
    Rect parent_frame = s->superview->content_layer_frame;
    uint32_t usable_height = (parent_frame.size.height / 6) * 5;
    uint32_t width = parent_frame.size.width * 0.8;
    uint32_t height = usable_height / 6;
    uint32_t mid_y = parent_frame.size.height / 2;
    return rect_make(
        point_make(
            (parent_frame.size.width / 2) - (width / 2),
            mid_y - (height / 2.0)
        ),
        size_make(
            width,
            height
        )
    );
}

static Rect _blue_slider_sizer(gui_slider_t* s, Size window_size) {
    Rect parent_frame = s->superview->content_layer_frame;
    uint32_t usable_height = (parent_frame.size.height / 6) * 5;
    uint32_t width = parent_frame.size.width * 0.8;
    uint32_t height = usable_height / 6;
    uint32_t mid_y = parent_frame.size.height / 2;
    return rect_make(
        point_make(
            (parent_frame.size.width / 2) - (width / 2),
            mid_y - (height / 2.0) + (height * 2)
        ),
        size_make(
            width,
            height
        )
    );
}

static Rect _apply_button_sizer(gui_button_t* b, Size window_size) {
    Rect parent_frame = b->superview->content_layer_frame;
    uint32_t t = window_size.width * 0.25;
    uint32_t w = parent_frame.size.width * 0.6;
    uint32_t h = parent_frame.size.height * 0.3;
    return rect_make(
        point_make(
            (parent_frame.size.width / 2) - (w / 2),
            (parent_frame.size.height / 2) - (h / 2)
        ),
        size_make(w, h)
    );
}

Color transcolor(Color c1, Color c2, float d) {
	if (d < 0) d = 0;
	if (d > 1) d = 1;
	return color_make(
		(c1.val[0] * (1 - d)) + (c2.val[0] * d),
		(c1.val[1] * (1 - d)) + (c2.val[1] * d),
		(c1.val[2] * (1 - d)) + (c2.val[2] * d)
	);
}

float pifdist(int x1, int y1, int x2, int y2) {
	float x = x1 - x2;
	float y = y1 - y2;
	return sqrt(x * x + y * y);
}

void _radial_gradient(gui_layer_t* layer, Size gradient_size, Color c1, Color c2, int x1, int y1, float r) {
	int x_step = gradient_size.width / 200.0;
	int y_step = gradient_size.height / 200.0;
    if (x_step < 1) x_step = 1;
    if (y_step < 1) y_step = 1;
	for (uint32_t y = 0; y < gradient_size.height; y += y_step) {
		for (uint32_t x = 0; x < gradient_size.width; x += x_step) {
			Color c = transcolor(c1, c2, pifdist(x1, y1, x, y) / r);
			for (int i = 0; i < x_step; i++) {
				for (int j = 0; j < y_step; j++) {
					putpixel(layer->fixed_layer.inner, x+i, y+j, c);
				}
			}
		}
	}
}

static void _render_slider_values(void) {
    gui_view_t* v = _g_state.preview;

    int r = _g_state.from_red->slider_percent * 255;
    int g = _g_state.from_green->slider_percent * 255;
    int b = _g_state.from_blue->slider_percent * 255;

    int r2 = _g_state.to_red->slider_percent * 255;
    int g2 = _g_state.to_green->slider_percent * 255;
    int b2 = _g_state.to_blue->slider_percent * 255;

    Size s = _g_state.preview->content_layer_frame.size;
    _radial_gradient(
        v->content_layer,
        s,
        color_make(b, g, r),
        color_make(b2, g2, r2),
        s.width/2.0, 
        s.height/2.0, 
        s.height * 1.3
    );
    _g_state.preview->_priv_needs_display = true;
}

void _slider_updated(gui_slider_t* sl, float new_percent) {
    _render_slider_values();
}

static void _apply_button_clicked(gui_view_t* view) {
    prefs_updated_msg_t msg = {0};
    msg.event = AWM_PREFERENCES_UPDATED;
    int r = _g_state.from_red->slider_percent * 255;
    int g = _g_state.from_green->slider_percent * 255;
    int b = _g_state.from_blue->slider_percent * 255;
    msg.from = color_make(b, g, r);

    int r2 = _g_state.to_red->slider_percent * 255;
    int g2 = _g_state.to_green->slider_percent * 255;
    int b2 = _g_state.to_blue->slider_percent * 255;
    msg.to = color_make(b2, g2, r2);

    amc_message_construct_and_send(AWM_SERVICE_NAME, &msg, sizeof(msg));
}

static void cb(void* ctx) {
    printf("CB Running at %d! 0x%08x\n", ms_since_boot(), ctx);
}

int main(int argc, char** argv) {
	amc_register_service(PREFERENCES_SERVICE_NAME);

	gui_window_t* window = gui_window_create("Preferences", 800, 360);
	Size window_size = window->size;

    // "From RGB" view & sliders
    _g_state.from_sliders_container = gui_view_create(window, (gui_window_resized_cb_t)_from_sliders_container_sizer);
    gui_view_set_title(_g_state.from_sliders_container, "Inner color RGB");
    _g_state.from_sliders_container->background_color = color_make(160, 160, 160);

    _g_state.from_red = gui_slider_create(_g_state.from_sliders_container, (gui_window_resized_cb_t)_red_slider_sizer);
    _g_state.from_red->slider_percent_updated_cb = _slider_updated;

    _g_state.from_green = gui_slider_create(_g_state.from_sliders_container, (gui_window_resized_cb_t)_green_slider_sizer);
    _g_state.from_green->slider_percent_updated_cb = _slider_updated;

    _g_state.from_blue = gui_slider_create(_g_state.from_sliders_container, (gui_window_resized_cb_t)_blue_slider_sizer);
    _g_state.from_blue->slider_percent_updated_cb = _slider_updated;

    // "To RGB" view & sliders
    _g_state.to_sliders_container = gui_view_create(window, (gui_window_resized_cb_t)_to_sliders_container_sizer);
    gui_view_set_title(_g_state.to_sliders_container, "Outer color RGB");
    _g_state.to_sliders_container->background_color = color_make(160, 160, 160);

    _g_state.to_red = gui_slider_create(_g_state.to_sliders_container, (gui_window_resized_cb_t)_red_slider_sizer);
    _g_state.to_red->slider_percent_updated_cb = _slider_updated;

    _g_state.to_green = gui_slider_create(_g_state.to_sliders_container, (gui_window_resized_cb_t)_green_slider_sizer);
    _g_state.to_green->slider_percent_updated_cb = _slider_updated;

    _g_state.to_blue = gui_slider_create(_g_state.to_sliders_container, (gui_window_resized_cb_t)_blue_slider_sizer);
    _g_state.to_blue->slider_percent_updated_cb = _slider_updated;

    // Preview view
    _g_state.preview = gui_view_create(window, (gui_window_resized_cb_t)_preview_sizer);
    _g_state.preview->controls_content_layer = true;
    gui_view_set_title(_g_state.preview, "Preview");

    // Apply view
    _g_state.apply_view = gui_view_create(window, (gui_window_resized_cb_t)_apply_view_sizer);
    _g_state.apply_view->background_color = color_make(160, 160, 160);
    _g_state.apply_button = gui_button_create(_g_state.apply_view, (gui_window_resized_cb_t)_apply_button_sizer, "Apply");
    _g_state.apply_button->button_clicked_cb = (gui_button_clicked_cb_t)_apply_button_clicked;

    // These match the values set up in awm
    // TODO(PT): How should these be read?
    Color to_initial = color_make(2, 184, 255);
    Color from_initial = color_make(39, 67, 255);
    _g_state.to_red->slider_percent = to_initial.val[0] / 255.0;
    _g_state.to_green->slider_percent = to_initial.val[1] / 255.0;
    _g_state.to_blue->slider_percent = to_initial.val[2] / 255.0;

    _g_state.from_red->slider_percent = from_initial.val[0] / 255.0;
    _g_state.from_green->slider_percent = from_initial.val[1] / 255.0;
    _g_state.from_blue->slider_percent = from_initial.val[2] / 255.0;
    _render_slider_values();

	gui_enter_event_loop(window);

	return 0;
}
