#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#include <agx/font/font.h>
#include <agx/lib/shapes.h>
#include <agx/lib/rect.h>
#include <libgui/libgui.h>
#include <stdlibadd/assert.h>

#define MAGNITUDE_SCALE 200

typedef struct point_f {
	float x;
	float y;
} point_f;

typedef struct vector {
	float theta;
	float mag;
} vector_t;

typedef struct game_brick_t {
	bool is_active;
	Rect frame;
	Color color;
} game_brick_t;

typedef struct game_state {
	gui_view_t* view;
	vector_t ball_vec;
	point_f ball_pos;
	uint32_t ball_radius;
	Rect paddle;
	uint32_t last_tick_time;

	game_brick_t bricks[128];
	uint32_t orig_bricks_count;

	bool physics_enabled;
	
	bool left_down;
	bool right_down;
} game_state_t;

static void draw_game_state(game_state_t* state);
static void _start_new_game(game_state_t* state);
static void _run_physics_tick(game_state_t* state);

static game_state_t state_s = {0};

static void _resize_elements(game_state_t* state) {
	Rect r = state->view->content_layer_frame;
	Size s = size_make(
		r.size.width / 6.0,
		r.size.height / 14.0
	);
	s.height = min(s.height, 16);
	state->paddle = rect_make(
		point_make(
			(r.size.width / 2.0) - (s.width / 2.0),
			(r.size.height) - (s.height * 2)
		), 
		s
	);

	state->ball_radius = 4;

	// TODO(PT): Move the ball to be above the paddle?
}

static void _window_resized(gui_view_t* view, Size new_size) {
	game_state_t* state = &state_s;
	_resize_elements(state);
	draw_game_state(state);
}

static Rect _game_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static void _key_up(gui_elem_t* elem, uint32_t ch) {
	game_state_t* state = &state_s;

	if (state->left_down && ch == KEY_IDENT_LEFT_ARROW) {
		state->left_down = false;
	}
	else if (state->right_down && ch == KEY_IDENT_RIGHT_ARROW) {
		state->right_down = false;
	}
}

static void _key_entered(gui_elem_t* elem, uint32_t ch) {
	game_state_t* state = &state_s;

	if (ch == KEY_IDENT_LEFT_ARROW) {
		// Move paddle left
		state->left_down = true;
	}

	else if (ch == KEY_IDENT_RIGHT_ARROW) {
		// Move paddle right
		state->right_down = true;
	}
	else if (ch == KEY_IDENT_DOWN_ARROW) {
		_start_new_game(state);
	}
	else if (ch == KEY_IDENT_UP_ARROW) {
		// Kick off the ball
		// pi / 4 		= bottom right
		// pi / 2 		= bottom
		// pi * 0.75	= bottom left
		// pi			= left
		// pi * 1.25 	= top left
		// pi * 1.5		= top
		// pi * 1.75 	= top right
		// pi * 2		= right
		// TODO(PT): The initial ball angle is a scoped random in the top 2 quadrants
		state->physics_enabled = true;
	}
	else if (ch == ' ') {
		// Pause or unpause as the case may be
		if (!state->physics_enabled) {
			state->physics_enabled = true;
		}
		else {
			state->physics_enabled = false;
			draw_game_state(state);
		}
	}
}

static Color _brick_color_for_row(uint32_t row_idx) {
	switch (row_idx) {
		case 0:
			return color_yellow();
			break;
		case 1:
			return color_green();
			break;
		case 2:
			return color_orange();
			break;
		case 3:
			return color_red();
			break;
		default:
			return color_white();
			break;
	}
}

static void _start_new_game(game_state_t* state) {
	srand(ms_since_boot());
	Rect r = rect_make(
		point_zero(),
		state->view->content_layer_frame.size
	);

	// Ensure the paddle is set up
	_resize_elements(state);

	state->ball_pos = (point_f){
		.x = (r.size.width / 2.0),
		.y = rect_min_y(state->paddle) - (state->ball_radius)
	};
	state->ball_vec.theta = M_PI_4 * 7;
	state->ball_vec.mag = MAGNITUDE_SCALE;
	//state->physics_enabled = true;

	Point brick_inset = point_make(state->ball_radius, state->ball_radius);
	Size bricks_frame = size_make(
		r.size.width - (brick_inset.x * 2),
		r.size.height / 3.0
	);
	uint32_t bricks_per_row = 12;
	uint32_t brick_rows = 4;

	Point brick_loc = brick_inset;
	Size brick_size = size_make(
		bricks_frame.width / (float)bricks_per_row,
		bricks_frame.height / (float)brick_rows
	);
	uint32_t brick_idx = 0;
	for (uint32_t row = 0; row < brick_rows; row++) {
		for (uint32_t brick_in_row = 0; brick_in_row < bricks_per_row; brick_in_row++) {
			state->bricks[brick_idx].is_active = true;
			state->bricks[brick_idx].color = _brick_color_for_row(row);
			state->bricks[brick_idx].frame = rect_make(brick_loc, brick_size);

			brick_loc.x += brick_size.width;
			brick_idx += 1;
		}
		brick_loc.x = brick_inset.x;
		brick_loc.y += brick_size.height;
	}
	state->orig_bricks_count = brick_idx + 1;
}

static void _vector_to_x_y(vector_t* vec, float* out_x, float* out_y) {
	*out_x = vec->mag * cos(vec->theta);
	*out_y = vec->mag * sin(vec->theta);
}

static void _vector_from_x_y(vector_t* out_vec, float x, float y) {
	// https://www.mathsisfun.com/algebra/vectors.html
	// r = √ ( x2 + y2 )
	// θ = tan-1 ( y / x )
	out_vec->mag = sqrtf(powf(x, 2) + powf(y, 2));
	out_vec->theta = atan2f(y, x);
}

static bool rect_intersects_rect(Rect r1, Rect r2) {
	// https://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other
	//return rect_min_x(r1) <= rect_max_x(r2) && rect_max_x(r1) >= rect_min_x(r2) &&
	//	rect_min_y(r1) >= rect_max_y(r2) && rect_max_y(r1) <= rect_min_y(r2);
	return (rect_min_x(r1) < rect_max_x(r2) &&
			rect_max_x(r1) > rect_min_x(r2) &&
			rect_min_y(r1) < rect_max_y(r2) &&
			rect_max_y(r1) > rect_min_y(r2));
}

static void _run_physics_tick(game_state_t* state) {
	uint32_t render_interval = 16;
	uint32_t now = ms_since_boot();
	uint32_t elapsed = now - state->last_tick_time;
	state->last_tick_time = now;
	float dt = (elapsed / 1000.0);

	if (!state->physics_enabled) {
		// And kick off a timer to continue the physics
		// TODO(PT): Support repeating timers
		gui_timer_start(state->view->window, render_interval, (gui_timer_cb_t)_run_physics_tick, state);
		return;
	}

	float diff_x, diff_y = 0;
	_vector_to_x_y(&state->ball_vec, &diff_x, &diff_y);
	diff_x *= dt;
	diff_y *= dt;

	state->ball_pos.x += diff_x;
	state->ball_pos.y += diff_y;

	float paddle_delta = 0;
	if (state->left_down) {
		paddle_delta = -200.0 * dt;
	}
	else if (state->right_down) {
		paddle_delta = 200.0 * dt;
	}
	if (paddle_delta != 0) {
		state->paddle.origin.x += paddle_delta;
		state->paddle.origin.x = max(0, state->paddle.origin.x);
		state->paddle.origin.x = max(state->paddle.origin.x, state->ball_radius);
		state->paddle.origin.x = min(
			state->paddle.origin.x, 
			state->view->content_layer_frame.size.width - state->paddle.size.width - state->ball_radius);
	}

	// Ball collision
	// Hit the right edge of the screen?
	Rect screen = rect_make(
		point_zero(),
		state->view->content_layer_frame.size
	);
	uint32_t min_x = state->ball_radius;
	uint32_t min_y = state->ball_radius;
	uint32_t max_x = screen.size.width - (state->ball_radius);
	uint32_t max_y = screen.size.height - (state->ball_radius);

	if (state->ball_pos.x < min_x || state->ball_pos.x >= max_x) {
		// Hit left or right screen edge
		state->ball_pos.x = max(min_x, state->ball_pos.x);
		state->ball_pos.x = min(max_x, state->ball_pos.x);

		// Negate X velocity
		float x, y = 0;
		_vector_to_x_y(&state->ball_vec, &x, &y);
		x = -x;
		_vector_from_x_y(&state->ball_vec, x, y);
	}
	else if (state->ball_pos.y < min_y || state->ball_pos.y >= max_y) {
		// Hit top or bottom screen edge
		// TODO(PT): If off bottom edge, you lose!
		state->ball_pos.y = max(min_y, state->ball_pos.y);
		state->ball_pos.y = min(max_y, state->ball_pos.y);

		// Negate Y velocity
		float x, y = 0;
		_vector_to_x_y(&state->ball_vec, &x, &y);
		y = -y;
		_vector_from_x_y(&state->ball_vec, x, y);
	}

	// Hit the paddle?
	Point ball_bottom = point_make(
		state->ball_pos.x,
		state->ball_pos.y + state->ball_radius
	);
	if (rect_contains_point(state->paddle, ball_bottom)) {
		// Negate Y velocity
		float x, y = 0;
		_vector_to_x_y(&state->ball_vec, &x, &y);
		y = -y;
		_vector_from_x_y(&state->ball_vec, x, y);
	}

	Rect ball_frame = rect_make(
		point_make(state->ball_pos.x, state->ball_pos.y),
		size_make(state->ball_radius*2, state->ball_radius*2)
	);
	for (uint32_t i = 0; i < state->orig_bricks_count; i++) {
		if (!state->bricks[i].is_active) {
			continue;
		}
		if (rect_intersects_rect(state->bricks[i].frame, ball_frame)) {
		//if (rect_contains_point(state->bricks[i].frame, point_make(state->ball_pos.x, state->ball_pos.y))) {
			state->bricks[i].is_active = false;
			// Negate Y velocity
			float x, y = 0;
			_vector_to_x_y(&state->ball_vec, &x, &y);
			y = -y;
			_vector_from_x_y(&state->ball_vec, x, y);
			break;
		}
	}

	draw_game_state(state);

	// And kick off a timer to continue the physics
	// TODO(PT): Support repeating timers
	gui_timer_start(state->view->window, render_interval, (gui_timer_cb_t)_run_physics_tick, state);
}

static void _draw_string(game_state_t* state, char* text, Point center, Size font_size) {
	uint32_t msg_len = strlen(text);
	uint32_t msg_width = msg_len * font_size.width;
	Point cursor = point_make(
		center.x - (msg_width / 2.0),
		center.y - (font_size.height / 2.0)
	);
	for (uint32_t i = 0; i < msg_len; i++) {
		draw_char(
			state->view->content_layer,
			text[i],
			cursor.x,
			cursor.y,
			color_white(),
			font_size
		);
		cursor.x += font_size.width;
	}
}

static void draw_game_state(game_state_t* state) {
	ca_layer* l = state->view->content_layer;
	Rect r = rect_make(
		point_zero(),
		state->view->content_layer_frame.size
	);

	// Fill a black background
	draw_rect(l, r, color_black(), THICKNESS_FILLED);

	// Draw the paddle
	draw_rect(l, state->paddle, color_white(), THICKNESS_FILLED);

	for (uint32_t i = 0; i < state->orig_bricks_count; i++) {
		if (!state->bricks[i].is_active) {
			continue;
		}
		draw_rect(
			l,
			state->bricks[i].frame,
			state->bricks[i].color,
			THICKNESS_FILLED
		);
		draw_rect(
			l,
			state->bricks[i].frame,
			color_light_gray(),
			1
		);
	}

	// Draw the ball
	draw_circle(
		l, 
		circle_make(
			point_make(
				(uint32_t)state->ball_pos.x,
				(uint32_t)state->ball_pos.y
			), 
			state->ball_radius
		),
		color_red(),
		THICKNESS_FILLED
	);
	//draw_rect(l, rect_make(point_make(state->ball_pos.x, state->ball_pos.y), size_make(state->ball_radius, state->ball_radius)), color_red(), THICKNESS_FILLED);

	if (!state->physics_enabled) {
		char* msg = "Press w to start!";
	/*
	Point cursor = point_make(
		rect_mid_x(r) - (msg_width / 2.0),
		rect_mid_y(r) - (font_size.height / 2.0)
	);
	*/
		Size font_size = size_make(8, 12);
		Point center = point_make(rect_mid_x(r), rect_mid_y(r));
		_draw_string(state, "Press up to start!", center, font_size);
		center.y += font_size.height;
		_draw_string(state, "Move left and right with arrow keys", center, font_size);
		center.y += font_size.height;
		_draw_string(state, "Press space to pause", center, font_size);
		center.y += font_size.height;
		_draw_string(state, "Press down to start a new game", center, font_size);
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.breakout");

	gui_window_t* window = gui_window_create("Breakout", 450, 180);
	Size window_size = window->size;

	gui_view_t* game_view = gui_view_create(
		window,
		(gui_window_resized_cb_t)_game_view_sizer
	);
	game_view->key_down_cb = (gui_key_down_cb_t)_key_entered;
	game_view->key_up_cb = (gui_key_up_cb_t)_key_up;
	game_view->controls_content_layer = true;
	game_view->window_resized_cb = (gui_window_resized_cb_t)_window_resized;

	game_state_t* state = &state_s;
	state->view = game_view;

	_start_new_game(state);
	//draw_game_state(state);
		// Start the physics runloop
		_run_physics_tick(state);

	gui_enter_event_loop(window);

	return 0;
}