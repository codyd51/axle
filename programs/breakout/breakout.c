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

#define MAGNITUDE_SCALE 300
#define BRICKS_PER_ROW 	12
#define BRICK_ROWS		4

typedef struct point_f {
	float x;
	float y;
} point_f;

typedef struct rect_f {
	point_f origin;
	Size size;
} rect_f;

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
	rect_f paddle;
	uint32_t last_tick_time;

	game_brick_t bricks[128];
	uint32_t orig_bricks_count;

	bool physics_enabled;
	
	bool left_down;
	bool right_down;

	uint32_t lives_remaining;
	uint32_t score;
} game_state_t;

static void _draw_hud(game_state_t* state);
static void draw_game_state(game_state_t* state);
static void _start_new_game(game_state_t* state);
static void _run_physics_tick(game_state_t* state);
static void _draw_string(game_state_t* state, char* text, Point center, Size font_size);
static void _draw_centered_string(game_state_t* state, char* text, Point center, Size font_size);

static game_state_t state_s = {0};

static Rect _hud_content_frame(game_state_t* state) {
	Size s = state->view->content_layer_frame.size;
	uint32_t hud_height = s.height / 14.0;
	hud_height = max(hud_height, 25);
	return rect_make(
		point_make(0, 0),
		size_make(s.width, hud_height)
	);
}

static Rect _game_content_frame(game_state_t* state) {
	Size s = state->view->content_layer_frame.size;
	uint32_t hud_height = s.height / 14.0;
	hud_height = max(hud_height, 25);
	return rect_make(
		point_make(0, hud_height),
		size_make(s.width, s.height - hud_height)
	);
}

static Rect _frame_for_brick_idx(game_state_t* state, uint32_t brick_idx) {
	Rect hud = _hud_content_frame(state);
	Rect r = _game_content_frame(state);
	Point bricks_origin = point_make(
		state->ball_radius,
		rect_max_y(hud) + state->ball_radius
	);
	Size bricks_frame = size_make(
		r.size.width - (state->ball_radius * 2),
		r.size.height / 3.0
	);

	Size brick_size = size_make(
		bricks_frame.width / (float)BRICKS_PER_ROW,
		bricks_frame.height / (float)BRICK_ROWS
	);

	uint32_t row = brick_idx / BRICKS_PER_ROW;
	uint32_t col = brick_idx % BRICKS_PER_ROW;
	return rect_make(
		point_make(
			bricks_origin.x + (brick_size.width * col),
			bricks_origin.y + (brick_size.height * row)
		),
		brick_size
	);
}

static void _resize_elements(game_state_t* state) {
	Rect r = state->view->content_layer_frame;
	Size s = size_make(
		r.size.width / 6.0,
		r.size.height / 14.0
	);
	s.height = min(s.height, 16);
	state->paddle = (rect_f){
		.origin = (point_f){
			.x = (r.size.width / 2.0) - (s.width / 2.0),
			.y = (r.size.height) - (s.height * 2)
		},
		.size = s
	};

	state->ball_radius = 4;

	for (uint32_t i = 0; i < state->orig_bricks_count; i++) {
		state->bricks[i].frame = _frame_for_brick_idx(state, i);
	}
}

static void _window_resized(gui_view_t* view, Size new_size) {
	game_state_t* state = &state_s;
	_resize_elements(state);
	_draw_hud(state);
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

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

static void _set_initial_ball_vector(game_state_t* state) {
	Rect r = _game_content_frame(state);

	state->ball_pos = (point_f){
		.x = rect_mid_x(state->paddle),
		.y = rect_min_y(state->paddle) - (state->ball_radius)
	};
	state->ball_vec.theta = M_PI_4 * 7;
	//state->ball_vec.theta = lerp(M_PI_4 * 5, M_PI_4 * 7, drand48());
	state->ball_vec.mag = MAGNITUDE_SCALE;
}

static void _start_new_game(game_state_t* state) {
	srand(ms_since_boot());

	state->lives_remaining = 3;

	// Place the bricks
	// (Must be done before the frames are set up in _resize_elements)
	state->orig_bricks_count = BRICKS_PER_ROW * BRICK_ROWS;
	for (uint32_t i = 0; i < state->orig_bricks_count; i++) {
		state->bricks[i].is_active = true;
		uint32_t row = i / BRICKS_PER_ROW;
		state->bricks[i].color = _brick_color_for_row(row);
	}

	// Ensure the paddle is set up
	_resize_elements(state);

	_set_initial_ball_vector(state);
	state->physics_enabled = false;

	_draw_hud(state);
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
	return (rect_min_x(r1) < rect_max_x(r2) &&
			rect_max_x(r1) > rect_min_x(r2) &&
			rect_min_y(r1) < rect_max_y(r2) &&
			rect_max_y(r1) > rect_min_y(r2));
}

static bool rect_f_intersects_rect(rect_f r1, Rect r2) {
	// https://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other
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
		// Kick off a timer to continue the physics
		gui_timer_start(render_interval, (gui_timer_cb_t)_run_physics_tick, state);
		return;
	}

	// If it's been a long time since the last frame,
	// for example if we paused physics,
	// don't try and update the physics models this time, 
	// because the dt will be wonky.
	if (elapsed > 100) {
		// Kick off a timer to continue the physics
		gui_timer_start(render_interval, (gui_timer_cb_t)_run_physics_tick, state);
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
		paddle_delta = -(float)MAGNITUDE_SCALE * dt;
	}
	else if (state->right_down) {
		paddle_delta = (float)MAGNITUDE_SCALE * dt;
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
	Rect screen = _game_content_frame(state);
	uint32_t min_x = rect_min_x(screen) + state->ball_radius;
	uint32_t min_y = rect_min_y(screen) + state->ball_radius;
	uint32_t max_x = rect_max_x(screen) - (state->ball_radius);
	uint32_t max_y = rect_max_y(screen) - (state->ball_radius);

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
		if (state->ball_pos.y >= max_y) {
			if (state->lives_remaining == 0) {
				_start_new_game(state);
				draw_game_state(state);
				_draw_hud(state);

				/*
				_draw_string(
					state, 
					"You lose!", 
					point_make(
						rect_mid_x(screen), 
						rect_mid_y(screen)
					),
					size_make(8, 12)
				);
				*/
			}
			else {
				state->lives_remaining -= 1;
				_set_initial_ball_vector(state);
				draw_game_state(state);
				_draw_hud(state);

				_draw_centered_string(
					state, 
					"Life lost!", 
					point_make(
						rect_mid_x(screen), 
						rect_mid_y(screen)
					),
					size_make(8, 12)
				);
			}
			gui_timer_start(1000, (gui_timer_cb_t)_run_physics_tick, state);
			return;
		}
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
	Rect ball_frame = rect_make(
		point_make(state->ball_pos.x - state->ball_radius, state->ball_pos.y - state->ball_radius),
		size_make(state->ball_radius*2, state->ball_radius*2)
	);

	if (rect_f_intersects_rect(state->paddle, ball_frame)) {
		// Negate Y velocity
		float x, y = 0;
		_vector_to_x_y(&state->ball_vec, &x, &y);
		y = -y;
		_vector_from_x_y(&state->ball_vec, x, y);

		// Make sure we always end up above the paddle instead of getting 'locked inside'
		state->ball_pos.y = min(state->ball_pos.y, rect_min_y(state->paddle) - state->ball_radius);
	}

	bool cleared_all_bricks = true;
	for (uint32_t i = 0; i < state->orig_bricks_count; i++) {
		if (!state->bricks[i].is_active) {
			continue;
		}
		cleared_all_bricks = false;
		if (rect_intersects_rect(state->bricks[i].frame, ball_frame)) {
			uint32_t row = i / BRICKS_PER_ROW;
			state->score += 200 + (200 * (BRICK_ROWS - row));
			// Draw the HUD now that we've updated the score
			_draw_hud(state);
			state->bricks[i].is_active = false;
			// Negate Y velocity
			float x, y = 0;
			_vector_to_x_y(&state->ball_vec, &x, &y);
			y = -y;
			_vector_from_x_y(&state->ball_vec, x, y);
			break;
		}
	}

	// Has the user won?
	if (cleared_all_bricks) {
		_start_new_game(state);
		draw_game_state(state);
		gui_timer_start(1000, (gui_timer_cb_t)_run_physics_tick, state);
		return;
	}

	draw_game_state(state);

	// And kick off a timer to continue the physics
	// TODO(PT): Support repeating timers
	gui_timer_start(render_interval, (gui_timer_cb_t)_run_physics_tick, state);
}

static void _draw_string(game_state_t* state, char* text, Point origin, Size font_size) {
	Point cursor = origin;
	for (uint32_t i = 0; i < strlen(text); i++) {
		gui_layer_draw_char(
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

static void _draw_centered_string(game_state_t* state, char* text, Point center, Size font_size) {
	uint32_t msg_len = strlen(text);
	uint32_t msg_width = msg_len * font_size.width;
	Point origin = point_make(
		center.x - (msg_width / 2.0),
		center.y - (font_size.height / 2.0)
	);
	_draw_string(state, text, origin, font_size);
}

static void _draw_hud(game_state_t* state) {
	ca_layer* l = state->view->content_layer;
	Rect r = _hud_content_frame(state);

	gui_layer_draw_rect(l, r, color_black(), THICKNESS_FILLED);

	Rect interior = rect_make(
		point_make(
			rect_min_x(r) + state->ball_radius,
			rect_min_y(r) + state->ball_radius
		),
		size_make(
			rect_max_x(r) - (state->ball_radius * 2),
			rect_max_y(r) - (state->ball_radius)
		)
	);
	gui_layer_draw_rect(l, interior, color_light_gray(), 1);

	uint32_t spacing = state->ball_radius * 3;
	Point lives_cursor = point_make(
		rect_min_x(interior) + spacing,
		rect_mid_y(interior)
	);
	for (uint32_t i = 0; i < state->lives_remaining; i++) {
		gui_layer_draw_circle(
			l, 
			circle_make(
				lives_cursor,
				state->ball_radius
			),
			color_white(),
			THICKNESS_FILLED
		);
		gui_layer_draw_circle(
			l, 
			circle_make(
				lives_cursor,
				state->ball_radius
			),
			color_light_gray(),
			1
		);
		lives_cursor.x += spacing;
	}

	char msg[128];
	snprintf(msg, sizeof(msg), "Score: %d", state->score);
	Size font_size = size_make(8, 12);
	Point center = point_make(rect_mid_x(r), rect_mid_y(r));
	Point origin = point_make(
		rect_max_x(interior) - ((strlen(msg) + 1) * font_size.width),
		rect_min_y(interior) + state->ball_radius
	);
	_draw_string(state, msg, origin, font_size);
}

static void draw_game_state(game_state_t* state) {
	gui_layer_t* l = state->view->content_layer;
	Rect r = _game_content_frame(state);

	// Fill a black background
	gui_layer_draw_rect(l, r, color_black(), THICKNESS_FILLED);

	// Draw the paddle
	Rect paddle_rect = rect_make(
		point_make(rect_min_x(state->paddle), rect_min_y(state->paddle)),
		state->paddle.size
	);
	gui_layer_draw_rect(l, paddle_rect, color_white(), THICKNESS_FILLED);
	gui_layer_draw_rect(l, paddle_rect, color_light_gray(), 2);

	for (uint32_t i = 0; i < state->orig_bricks_count; i++) {
		if (!state->bricks[i].is_active) {
			continue;
		}
		gui_layer_draw_rect(
			l,
			state->bricks[i].frame,
			state->bricks[i].color,
			THICKNESS_FILLED
		);
		gui_layer_draw_rect(
			l,
			state->bricks[i].frame,
			color_light_gray(),
			1
		);
	}

	// Draw the ball
	gui_layer_draw_circle(
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

	if (!state->physics_enabled) {
		char* msg = "Press w to start!";
		Size font_size = size_make(8, 12);
		Point center = point_make(rect_mid_x(r), rect_mid_y(r));
		_draw_centered_string(state, "Press up to start!", center, font_size);
		center.y += font_size.height;
		_draw_centered_string(state, "Move left and right with arrow keys", center, font_size);
		center.y += font_size.height;
		_draw_centered_string(state, "Press space to pause", center, font_size);
		center.y += font_size.height;
		_draw_centered_string(state, "Press down to start a new game", center, font_size);
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.breakout");

	gui_window_t* window = gui_window_create("Breakout", 450, 200);
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
	// Start the physics runloop
	_run_physics_tick(state);

	gui_enter_event_loop();

	return 0;
}
