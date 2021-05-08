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

#define ROWS 16
#define COLS 16
#define SQUARE_COUNT (ROWS * COLS)

typedef struct game_square {
	bool contains_snake;
	uint32_t ttl;

	bool contains_treat;
} game_square_t;

typedef enum direction {
	DIRECTION_LEFT = 0,
	DIRECTION_RIGHT = 1,
	DIRECTION_UP = 2,
	DIRECTION_DOWN = 3
} direction_t;

typedef struct game_state {
	gui_view_t* view;
	game_square_t squares[SQUARE_COUNT];
	game_square_t* head_square;
	direction_t direction;
	uint32_t snake_length;
	bool processing_treat;
	array_t* queued_moves;
	uint32_t lose_screen_state;
} game_state_t;

static void _run_tick(game_state_t* state);

static game_state_t state_s = {0};

static Rect _game_content_frame(game_state_t* state) {
	Size s = state->view->content_layer_frame.size;
	return rect_make(
		point_make(0, 0),
		s
	);
}

static void _window_resized(gui_view_t* view, Size new_size) {
	game_state_t* state = &state_s;
	/*
	_resize_elements(state);
	draw_game_state(state);
	*/
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

static Rect _game_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static void _enqueue_move(game_state_t* state, direction_t move) {
	array_insert(state->queued_moves, (type_t)move);
}

static void _key_entered(gui_elem_t* elem, uint32_t ch) {
	game_state_t* state = &state_s;

	if (ch == KEY_IDENT_LEFT_ARROW) {
		_enqueue_move(state, DIRECTION_LEFT);
	}
	else if (ch == KEY_IDENT_RIGHT_ARROW) {
		_enqueue_move(state, DIRECTION_RIGHT);
	}
	else if (ch == KEY_IDENT_UP_ARROW) {
		_enqueue_move(state, DIRECTION_UP);
	}
	else if (ch == KEY_IDENT_DOWN_ARROW) {
		_enqueue_move(state, DIRECTION_DOWN);
	}
}

static game_square_t* _game_square_get(game_state_t* state, uint32_t row, uint32_t col) {
	uint32_t idx = (row * COLS) + col;
	return &state->squares[idx];
}

static void _spawn_treat(game_state_t* state) {
	// Build an array containing indexes of free tiles
	uint32_t free_square_idxs[SQUARE_COUNT] = {0};
	uint32_t free_square_count = 0;
	for (uint32_t i = 0; i < SQUARE_COUNT; i++) {
		game_square_t* square = &state->squares[i];
		if (!square->contains_snake && !square->contains_treat) {
			free_square_idxs[free_square_count] = i;
			free_square_count += 1;
		}
	}

	// Now, pick a random free square to spawn the treat into
	uint32_t chosen_square_idx = free_square_idxs[rand() % free_square_count];
	game_square_t* chosen_square = &state->squares[chosen_square_idx];
	chosen_square->contains_treat = true;
}

static void _start_new_game(game_state_t* state) {
	srand(ms_since_boot());

	printf("_start_new_game 0x%08x 0x%08x\n", state, sizeof(game_state_t));

	gui_view_t* v = state->view;
	array_t* q = state->queued_moves;
	memset(state, 0, sizeof(game_state_t));
	state->view = v;
	state->queued_moves = q;

	state->snake_length = 1;
	state->head_square = _game_square_get(state, COLS / 2, ROWS / 2);
	state->head_square->contains_snake = true;
	state->head_square->ttl = state->snake_length;

	_spawn_treat(state);
}

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

Color lerp_color(Color a, Color b, float f) {
	return color_make(
		lerp(a.val[2], b.val[2], f),
		lerp(a.val[1], b.val[1], f),
		lerp(a.val[0], b.val[0], f)
	);
}

static void draw_game_state(game_state_t* state) {
	ca_layer* l = state->view->content_layer;
	Rect r = _game_content_frame(state);

	// Fill a black background
	draw_rect(l, r, color_black(), THICKNESS_FILLED);

	Size square_size = size_make(
		r.size.width / (float)COLS,
		r.size.height / (float)ROWS
	);

	for (uint32_t row = 0; row < COLS; row++) {
		for (uint32_t col = 0; col < ROWS; col++) {
			uint32_t idx = (row * COLS) + col;
			game_square_t* square = &state->squares[idx];
			Point origin = point_make(
				col * square_size.width,
				row * square_size.height
			);
			Rect sr = rect_make(origin, square_size);

			if (square->contains_treat) {
				//draw_rect(l, sr, color_red(), THICKNESS_FILLED);
				Point center = point_make(rect_mid_x(sr), rect_mid_y(sr));
				Circle c = circle_make(center, sr.size.width/2.5);
				draw_circle(l, c, color_red(), THICKNESS_FILLED);
				draw_circle(l, c, color_make(140, 70, 70), 2);
			}
			else if (square->contains_snake) {
				if (square == state->head_square) {
					draw_rect(l, sr, color_green(), THICKNESS_FILLED);
					//draw_rect(l, inset, color_white(), 1);
				}
				else {
					float inset_x, inset_y = 0;
					//inset_x = sr.size.width / 20.0;
					//inset_y = sr.size.height / 20.0;
					inset_x = 1;
					inset_y = 1;
					Rect inset = rect_make(
						point_make(rect_min_x(sr) + inset_x, rect_min_y(sr) + inset_y),
						size_make(sr.size.width - (inset_x * 2), sr.size.height - (inset_y * 2))
					);
					Color c = lerp_color(color_make(255, 100, 0), color_yellow(), (square->ttl + 1) / (float)(state->snake_length - 1));
					draw_rect(l, inset, c, THICKNESS_FILLED);
					//draw_rect(l, inset, color_light_gray(), 1);
				}
			}

		}
	}
}

static void _flash_lose_screen(game_state_t* state) {
	Rect r = _game_content_frame(state);
	Point center = point_make(rect_mid_x(r), rect_mid_y(r));
	Size font_size = size_make(16, 24);
	switch (state->lose_screen_state) {
		case 0:
			draw_game_state(state);
			_draw_string(state, "You lose!", center, font_size);
			state->lose_screen_state += 1;
			gui_timer_start(state->view->window, 500, (gui_timer_cb_t)_flash_lose_screen, state);
			return;
			break;
		case 1:
			draw_game_state(state);
			state->lose_screen_state += 1;
			gui_timer_start(state->view->window, 500, (gui_timer_cb_t)_flash_lose_screen, state);
			return;
			break;
		case 2:
			draw_game_state(state);
			_draw_string(state, "You lose!", center, font_size);
			state->lose_screen_state += 1;
			gui_timer_start(state->view->window, 500, (gui_timer_cb_t)_flash_lose_screen, state);
			return;
			break;
		case 3:
			draw_game_state(state);
			state->lose_screen_state += 1;
			gui_timer_start(state->view->window, 500, (gui_timer_cb_t)_flash_lose_screen, state);
			return;
			break;
		case 4:
			draw_game_state(state);
			_draw_string(state, "You lose!", center, font_size);
			state->lose_screen_state += 1;
			gui_timer_start(state->view->window, 500, (gui_timer_cb_t)_flash_lose_screen, state);
			return;
			break;
		case 5:
			_start_new_game(state);
			draw_game_state(state);
			gui_timer_start(state->view->window, 500, (gui_timer_cb_t)_run_tick, state);
			return;
			break;
	}
}

static void _run_tick(game_state_t* state) {
	uint32_t tick_interval = 80;
	draw_game_state(state);

	// Apply a move in the queue, if any
	while (state->queued_moves->size) {
		direction_t queued_move = (direction_t)array_lookup(state->queued_moves, 0);
		array_remove(state->queued_moves, 0);
		// We can't directly flip direction
		if ((state->direction == DIRECTION_UP && queued_move == DIRECTION_DOWN) ||
			(state->direction == DIRECTION_DOWN && queued_move == DIRECTION_UP) ||
			(state->direction == DIRECTION_LEFT && queued_move == DIRECTION_RIGHT) ||
			(state->direction == DIRECTION_RIGHT && queued_move == DIRECTION_LEFT)) {
			continue;
		}

		state->direction = queued_move;
		break;
	}

	// Move in the direction of motion
	uint32_t head_idx = state->head_square - state->squares;
	uint32_t head_row = head_idx / COLS;
	uint32_t head_col = head_idx % COLS;
	int32_t new_head_idx = 0;

	if (state->direction == DIRECTION_LEFT) {
		new_head_idx = (head_row * COLS) + (head_col - 1);
		if (head_col == 0) {
			new_head_idx = (head_row * COLS) + (COLS - 1);
		}
	}
	else if (state->direction == DIRECTION_RIGHT) {
		new_head_idx = (head_row * COLS) + (head_col + 1);
		if (head_col == COLS - 1) {
			new_head_idx = (head_row * COLS);
		}
	}
	else if (state->direction == DIRECTION_UP) {
		new_head_idx = ((head_row - 1) * COLS) + head_col;
		if (new_head_idx < 0) {
			new_head_idx = ((ROWS - 1) * COLS) + head_col;
		}
	}
	else if (state->direction == DIRECTION_DOWN) {
		new_head_idx = ((head_row + 1) * COLS) + head_col;
		if (new_head_idx >= SQUARE_COUNT) {
			new_head_idx = ((0) * COLS) + head_col;
		}
	}

	// Iterate every prior square and decrement its ttl
	if (!state->processing_treat) {
		for (uint32_t i = 0; i < SQUARE_COUNT; i++) {
			if (state->squares[i].contains_snake) {
				if (state->squares[i].ttl == 0) {
					state->squares[i].contains_snake = false;
					state->snake_length -= 1;
				}
				else {
					state->squares[i].ttl -= 1;
				}
			}
		}
	}
	state->processing_treat = false;

	// Place the new head square
	state->head_square = &state->squares[new_head_idx];

	// Have we hit ourselves?
	if (state->head_square->contains_snake) {
		_flash_lose_screen(state);
		return;
	}

	state->head_square->contains_snake = true;
	state->head_square->ttl = state->snake_length;
	state->snake_length += 1;

	// Have we eaten a treat?
	if (state->head_square->contains_treat) {
		_spawn_treat(state);
		state->head_square->contains_treat = false;
		printf("Ate treat!\n");
		state->processing_treat = true;
	}

	// Kick off a timer to continue the physics
	gui_timer_start(state->view->window, tick_interval, (gui_timer_cb_t)_run_tick, state);
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.snake");

	gui_window_t* window = gui_window_create("Snake", 300, 300);
	Size window_size = window->size;

	gui_view_t* game_view = gui_view_create(
		window,
		(gui_window_resized_cb_t)_game_view_sizer
	);
	game_view->key_down_cb = (gui_key_down_cb_t)_key_entered;
	game_view->controls_content_layer = true;
	game_view->window_resized_cb = (gui_window_resized_cb_t)_window_resized;

	game_state_t* state = &state_s;
	state->view = game_view;
	state->queued_moves = array_create(32);

	_start_new_game(state);
	_run_tick(state);

	gui_enter_event_loop(window);

	return 0;
}
