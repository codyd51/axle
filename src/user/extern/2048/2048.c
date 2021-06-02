#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <agx/font/font.h>
#include <agx/lib/shapes.h>
#include <agx/lib/rect.h>
#include <libgui/libgui.h>
#include <stdlibadd/assert.h>

#define TILE_COUNT	16
#define TILES_PER_ROW	4
#define TILES_PER_COL	4
#define TILE_ROWS (TILE_COUNT / TILES_PER_ROW)
#define TILE_COLS (TILE_COUNT / TILES_PER_COL)

#define ANIMATIONS_ENABLED 

typedef enum slide_direction {
	SLIDE_DIRECTION_LEFT = 0,
	SLIDE_DIRECTION_RIGHT = 1,
	SLIDE_DIRECTION_UP = 2,
	SLIDE_DIRECTION_DOWN = 3,
} slide_direction_t;

typedef enum slide_action {
	SLIDE_TO_EMPTY_SQUARE = 0,
	SLIDE_AND_COMBINE = 1,
	SLIDE_NOT_ALLOWED = 2
} slide_action_t;

typedef struct game_tile {
	bool is_occupied;
	uint32_t value;
	bool already_combined_this_turn;
} game_tile_t;

typedef struct game_tile_draw_info {
	Color color;
	Color font_color;
	bool is_occupied;
	uint32_t value;
} game_tile_draw_info_t;

typedef enum tile_animation_type {
	TILE_ANIMATION_TYPE_MOVE = 1
} tile_animation_type_t;

typedef struct tile_base_animation {
	// Common fields
	tile_animation_type_t type;
	uint32_t start_time;
	uint32_t end_time;
	game_tile_draw_info_t draw_info;
} tile_base_animation_t;

typedef struct tile_move_animation {
	// Common fields
	tile_animation_type_t type;
	uint32_t start_time;
	uint32_t end_time;
	game_tile_draw_info_t draw_info;
	// Unique fields
	uint32_t tile_pos;
	Rect from;
	Rect to;
} tile_move_animation_t;

typedef union tile_animation {
	tile_base_animation_t base;
	tile_move_animation_t move;
} tile_animation_t;

typedef struct game_board_animation_state {
	bool animations_ongoing;
	array_t* pending_animations;
	bool tile_being_animated[TILE_COUNT];
} game_board_animation_state_t;


typedef struct game_state {
	gui_view_t* view;
	game_tile_t tiles[TILE_COUNT];
	game_board_animation_state_t animation_state;
} game_state_t;

static void draw_game_state(game_state_t* state);

game_state_t state_s = {0};

static void _window_resized(gui_view_t* view, Size new_size) {
	// TODO(PT): Might need a draw wrapper here that draws state or animations
	draw_game_state(&state_s);
}

static void _get_separator_size(Rect game_frame, uint32_t* out_col_separator, uint32_t* out_row_separator) {
	uint32_t col_separator_width = game_frame.size.width / 40.0;
	col_separator_width = max(8, col_separator_width);
	*out_col_separator = col_separator_width;

	uint32_t row_separator_width = game_frame.size.height / 40.0;
	row_separator_width = max(8, row_separator_width);
	*out_row_separator = row_separator_width;
}

static Rect _tile_rect_from_idx(Rect game_frame, uint32_t tile_idx) {
	uint32_t col_separator, row_separator = 0;
	_get_separator_size(game_frame, &col_separator, &row_separator);

	Rect usable_frame = rect_make(
		point_zero(),
		size_make(
			game_frame.size.width - col_separator,
			game_frame.size.height - row_separator
		)
	);

	uint32_t row = tile_idx % TILES_PER_ROW;
	uint32_t col = tile_idx / TILES_PER_ROW;
	uint32_t tile_width = usable_frame.size.width / TILES_PER_ROW;
	uint32_t tile_height = usable_frame.size.height / TILES_PER_COL;

	Rect r = rect_make(
		point_make(
			col_separator + (tile_width * row),
			row_separator + (tile_height * col)
		),
		size_make(tile_width - col_separator, tile_height - row_separator)
	);
	return r;
}

static game_tile_draw_info_t _tile_draw_info(game_tile_t* tile) {
	game_tile_draw_info_t info;
	info.font_color = color_white();
	info.is_occupied = tile->is_occupied;
	info.value = tile->value;

	if (!tile->is_occupied) {
		info.color = color_make(208, 201, 193);
		return info;
	}

	switch (tile->value) {
		case 2:
			info.font_color = color_make(117, 110, 102);
			info.color = color_make(236, 228, 219);
			break;
		case 4:
			info.font_color = color_make(117, 110, 102);
			info.color = color_make(235, 224, 203);
			break;
		case 8:
			info.color = color_make(232, 179, 129);
			break;
		case 16:
			info.color = color_make(232, 153, 108);
			break;
		case 32:
			info.color = color_make(230, 130, 103);
			break;
		case 64:
			info.color = color_make(228, 103, 70);
			break;
		case 128:
			info.color = color_make(232, 206, 128);
			break;
		case 256:
			info.color = color_make(231, 202, 116);
			break;
		case 512:
			info.color = color_make(231, 199, 101);
			break;
		case 1024:
			info.color = color_make(232, 199, 93);
			break;
		case 2048:
			info.color = color_make(229, 195, 80);
			break;
		case 4096:
			info.color = color_make(233, 79, 72);
			break;
		default:
			info.color = color_make(233, 61, 50);
			break;
	}
	return info;
}

static void _draw_tile_with_params(game_state_t* state, game_tile_draw_info_t draw_info, Rect r) {
	gui_layer_t* l = state->view->content_layer;

	gui_layer_draw_rect(l, r, draw_info.color, THICKNESS_FILLED);

	if (draw_info.is_occupied) {
		char buf[64];
		snprintf(buf, sizeof(buf), "%d", draw_info.value);


		Size font_size = size_make(8, 8);
		font_size.width = max(font_size.width, r.size.width / 4.0);
		font_size.height = max(font_size.height, r.size.height / 3.0);

		uint32_t text_width = strlen(buf) * font_size.width;
		uint32_t center_x = rect_max_x(r) - (r.size.width / 2.0);
		uint32_t center_y = rect_max_y(r) - (r.size.height / 2.0);

		Point text_origin = point_make(
			center_x - (text_width / 2.0),
			center_y - (font_size.height / 2.0)
		);
		Point cursor = text_origin;
		for (uint32_t j = 0; j < strlen(buf); j++) {
			gui_layer_draw_char(l, buf[j], cursor.x, cursor.y, draw_info.font_color, font_size);
			cursor.x += font_size.width;
		}
	}
}

static void _draw_tile(game_state_t* state, uint32_t tile_idx) {
	Rect frame = state->view->content_layer_frame;
	game_tile_t* tile = &state->tiles[tile_idx];
	game_tile_draw_info_t draw_info = _tile_draw_info(tile);
	Rect r = _tile_rect_from_idx(frame, tile_idx);
	_draw_tile_with_params(state, draw_info, r);
}

static void _draw_separators(game_state_t* state) {
	gui_layer_t* l = state->view->content_layer;

	uint32_t col_separator, row_separator = 0;
	_get_separator_size(state->view->content_layer_frame, &col_separator, &row_separator);

	Rect frame = rect_make(
		point_zero(),
		size_make(
			state->view->content_layer_frame.size.width - col_separator,
			state->view->content_layer_frame.size.height - row_separator
		)
	);

	Point origin = point_make(col_separator, row_separator);

	Color separator_color = color_make(181, 169, 159);
	// Draw the vertical row seperator lines
	for (uint32_t i = 0; i < TILES_PER_ROW + 1; i++) {
		Rect r = rect_make(
			point_make(
				i * (frame.size.width / TILES_PER_ROW),
				0
			),
			size_make(
				col_separator,
				frame.size.height
			)
		);
		gui_layer_draw_rect(l, r, separator_color, THICKNESS_FILLED);
	}
	// Draw the horizontal column seperator lines
	for (uint32_t i = 0; i < TILES_PER_COL + 1; i++) {
		Rect r = rect_make(
			point_make(
				0,
				i * (frame.size.height / TILES_PER_COL)
			),
			size_make(
				frame.size.width,
				row_separator
			)
		);
		gui_layer_draw_rect(l, r, separator_color, THICKNESS_FILLED);
	}
}

static void draw_game_state(game_state_t* state) {
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		_draw_tile(state, i);
	}
	_draw_separators(state);
}

static Rect _game_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static slide_action_t _slide_possibility(game_state_t* state, game_tile_t* tile, uint32_t tile_idx, game_tile_t* destination, uint32_t destination_idx, slide_direction_t direction) {
	assert(tile->is_occupied, "Expected occupied tile");
	if (destination->is_occupied) {
		if (tile->value == destination->value) {
			if (!tile->already_combined_this_turn && !destination->already_combined_this_turn) {
				// Are there intervening tiles between these that block the combination?
				if (direction == SLIDE_DIRECTION_LEFT) {
					// Tile is to the right of destination
					assert(tile_idx > destination_idx, "Expected tile_idx > destination_idx");
					assert(tile_idx - destination_idx <= TILES_PER_ROW, "Expecetd tile_idx and destination_idx to be in the same row");
					for (uint32_t i = tile_idx - 1; i > destination_idx; i--) {
						game_tile_t* in_between_tile = &state->tiles[i];
						if (in_between_tile->is_occupied) {
							return SLIDE_NOT_ALLOWED;
						}
					}
				}
				else if (direction == SLIDE_DIRECTION_RIGHT) {
					// Tile is to the left of destination
					assert(tile_idx < destination_idx, "Expected tile_idx < destination_idx");
					assert(destination_idx - tile_idx <= TILES_PER_ROW, "Expecetd tile_idx and destination_idx to be in the same row");
					for (uint32_t i = destination_idx - 1; i > tile_idx; i--) {
						game_tile_t* in_between_tile = &state->tiles[i];
						if (in_between_tile->is_occupied) {
							return SLIDE_NOT_ALLOWED;
						}
					}
				}
				else if (direction == SLIDE_DIRECTION_UP) {
					// Tile is below the destination
					assert(tile_idx > destination_idx, "Expected tile_idx > destination_idx");
					int32_t tile_row = tile_idx / (int)TILES_PER_ROW;
					int32_t dest_row = destination_idx / (int)TILES_PER_ROW;
					assert(tile_row > dest_row, "Expected tile row > dest row");
					assert(tile_row / TILES_PER_COL == dest_row / TILES_PER_COL, "Expected same column");

					for (int32_t i = tile_idx - TILES_PER_ROW; i > destination_idx; i -= TILES_PER_ROW) {
						game_tile_t* in_between_tile = &state->tiles[i];
						if (in_between_tile->is_occupied) {
							return SLIDE_NOT_ALLOWED;
						}
					}
				}
				else if (direction == SLIDE_DIRECTION_DOWN) {
					// Tile is above the destination
					assert(tile_idx < destination_idx, "Expected tile_idx < destination_idx");
					int32_t tile_row = tile_idx / (int)TILES_PER_ROW;
					int32_t dest_row = destination_idx / (int)TILES_PER_ROW;
					assert(tile_row < dest_row, "Expected tile row < dest row");
					assert(tile_row / TILES_PER_COL == dest_row / TILES_PER_COL, "Expected same column");

					for (int32_t i = destination_idx - TILES_PER_ROW; i > tile_idx; i -= TILES_PER_ROW) {
						game_tile_t* in_between_tile = &state->tiles[i];
						if (in_between_tile->is_occupied) {
							return SLIDE_NOT_ALLOWED;
						}
					}
				}
				else {
					assert(false, "Unknown slide direction");
				}
				return SLIDE_AND_COMBINE;
			}
		}
		return SLIDE_NOT_ALLOWED;
	}
	return SLIDE_TO_EMPTY_SQUARE;
}

static void _reset_turn_flags(game_state_t* state) {
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		state->tiles[i].already_combined_this_turn = false;
	}
}

static bool _try_slide(game_state_t* state, game_tile_t* tile, uint32_t tile_idx, game_tile_t* destination, uint32_t destination_idx, slide_direction_t direction) {
	slide_action_t result = _slide_possibility(state, tile, tile_idx, destination, destination_idx, direction);
	if (result == SLIDE_TO_EMPTY_SQUARE) {
		tile->is_occupied = false;
		destination->is_occupied = true;
		destination->value = tile->value;
		return true;
	}
	else if (result == SLIDE_AND_COMBINE) {
		tile->is_occupied = false;
		destination->value += tile->value;
		destination->already_combined_this_turn = true;
		return true;
	}
	else if (result == SLIDE_NOT_ALLOWED) {
		return false;
	}
	assert(false, "Unknown slide possibility");
}

static void _enqueue_move_animation(game_state_t* state,  uint32_t tile_idx, uint32_t duration, Rect from) {
	// Only enqueue an animation if we've enabled animations
#ifdef ANIMATIONS_ENABLED
	game_tile_t* destination_tile = &state->tiles[tile_idx];

	tile_move_animation_t* anim = calloc(1, sizeof(tile_move_animation_t));
	anim->type = TILE_ANIMATION_TYPE_MOVE;
	anim->start_time = ms_since_boot();
	anim->end_time = anim->start_time + duration;
	anim->draw_info = _tile_draw_info(destination_tile);
	anim->tile_pos = tile_idx;

	anim->from = from;
	anim->to = _tile_rect_from_idx(state->view->content_layer_frame, tile_idx);

	state->animation_state.tile_being_animated[anim->tile_pos] = true;
	state->animation_state.animations_ongoing = true;
	array_insert(state->animation_state.pending_animations, anim);
#endif
}

static bool slide_tile_left(game_state_t* state, uint32_t tile_idx) {
	game_tile_t* tile = &state->tiles[tile_idx];
	if (!tile->is_occupied) {
		return false;
	}
	uint32_t idx_within_row = tile_idx % TILES_PER_COL;
	if (idx_within_row == 0) {
		return false;
	}
	// Check each tile to the left of this one to see if we can slide into it
	// And start from the leftmost position
	uint32_t row_start = tile_idx - idx_within_row;
	for (uint32_t i = row_start; i < tile_idx; i++) {
		game_tile_t* destination_tile = &state->tiles[i];
		if (_try_slide(state, tile, tile_idx, destination_tile, i, SLIDE_DIRECTION_LEFT)) {
			// We moved the tile in the model.
			// Generate an animation to reflect the move
			uint32_t distance = tile_idx - i;
			uint32_t duration = 80 + (100 / (float)(TILES_PER_ROW - distance));
			Rect from = _tile_rect_from_idx(state->view->content_layer_frame, tile_idx);
			_enqueue_move_animation(state, i, duration, from);
			return true;
		}
	}
	return false;
}

static bool left_slide_all_tiles(game_state_t* state) {
	// Iterate each row from left to right, sliding each tile as far leftwards as we can
	bool found_tile_to_move = false;
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		if (slide_tile_left(state, i)) {
			found_tile_to_move = true;
		}
	}

	return found_tile_to_move;
}


static bool slide_tile_right(game_state_t* state, uint32_t tile_idx, uint32_t row_end) {
	game_tile_t* tile = &state->tiles[tile_idx];
	if (!tile->is_occupied) {
		return false;
	}
	uint32_t idx_within_row = tile_idx % TILES_PER_COL;
	if (idx_within_row == TILES_PER_ROW - 1) {
		return false;
	}
	// Check each tile to the right of this one to see if we can slide into it
	for (int32_t i = row_end; i >= tile_idx + 1; i--) {
		game_tile_t* destination_tile = &state->tiles[i];
		if (_try_slide(state, tile, tile_idx, destination_tile, i, SLIDE_DIRECTION_RIGHT)) {
			// We moved the tile in the model.
			// Generate an animation to reflect the move
			uint32_t distance = i - tile_idx;
			uint32_t duration = 80 + (100 / (float)(TILES_PER_ROW - distance));
			Rect from = _tile_rect_from_idx(state->view->content_layer_frame, tile_idx);
			_enqueue_move_animation(state, i, duration, from);
			return true;
		}
	}
	return false;
}

static bool right_slide_all_tiles(game_state_t* state) {
	// Iterate each row from right to left, sliding each tile as far rightwards as we can
	bool found_tile_to_move = false;
	for (uint32_t i = 0; i < TILE_ROWS; i++) {
		int32_t row_start = (i * TILES_PER_ROW);
		int32_t row_end = ((i + 1) * TILES_PER_ROW) - 1;
		// No need to try sliding the rightmost tile in a row
		for (int32_t j = row_end - 1; j >= row_start; j--) {
			if (slide_tile_right(state, j, row_end)) {
				found_tile_to_move = true;
			}
		}
	}
	return found_tile_to_move;
}

static bool slide_tile_up(game_state_t* state, uint32_t tile_idx, int32_t col_start) {
	game_tile_t* tile = &state->tiles[tile_idx];
	if (!tile->is_occupied) {
		return false;
	}
	uint32_t idx_within_col = tile_idx / (int)TILES_PER_ROW;
	if (idx_within_col == 0) {
		return false;
	}
	// Check each tile above this one to see if we can slide into it
	// Start from the top
	for (int32_t i = col_start; i < tile_idx; i += TILES_PER_ROW) {
		game_tile_t* destination_tile = &state->tiles[i];
		if (_try_slide(state, tile, tile_idx, destination_tile, i, SLIDE_DIRECTION_UP)) {
			// We moved the tile in the model.
			// Generate an animation to reflect the move
			uint32_t dst_idx_within_col = i / (int)TILES_PER_ROW;
			uint32_t distance = dst_idx_within_col - idx_within_col;
			uint32_t duration = 80 + (100 / (float)(TILES_PER_COL - distance));
			Rect from = _tile_rect_from_idx(state->view->content_layer_frame, tile_idx);
			_enqueue_move_animation(state, i, duration, from);
			return true;
		}
	}
	return false;
}


static bool up_slide_all_tiles(game_state_t* state) {
	// Iterate each column from top to bottom, sliding each tile as far upwards as we can
	bool found_tile_to_move = false;
	for (uint32_t col_start = 0; col_start < TILE_COLS; col_start++) {
		// No need to try sliding the topmost tile in a column
		uint32_t col_end = col_start + (TILES_PER_ROW * (TILE_COLS - 1));
		for (int32_t i = col_start + TILES_PER_ROW; i <= col_end; i += TILES_PER_ROW) {
			if (slide_tile_up(state, i, col_start)) {
				found_tile_to_move = true;
			}
		}
	}
	return found_tile_to_move;
}

static bool slide_tile_down(game_state_t* state, uint32_t tile_idx, int32_t col_end) {
	game_tile_t* tile = &state->tiles[tile_idx];
	if (!tile->is_occupied) {
		return false;
	}
	uint32_t idx_within_col = tile_idx / (int)TILES_PER_ROW;
	if (idx_within_col == TILE_ROWS - 1) {
		return false;
	}
	// Check each tile below this one to see if we can slide into it
	// Start from the bottom
	for (int32_t i = col_end; i > tile_idx; i -= TILES_PER_ROW) {
		game_tile_t* destination_tile = &state->tiles[i];
		if (i == tile_idx) continue;
		if (_try_slide(state, tile, tile_idx, destination_tile, i, SLIDE_DIRECTION_DOWN)) {
			// We moved the tile in the model.
			// Generate an animation to reflect the move
			uint32_t dst_idx_within_col = i / (int)TILES_PER_ROW;
			uint32_t distance = dst_idx_within_col - idx_within_col;
			uint32_t duration = 80 + (100 / (float)(TILES_PER_COL - distance));
			Rect from = _tile_rect_from_idx(state->view->content_layer_frame, tile_idx);
			_enqueue_move_animation(state, i, duration, from);
			return true;
		}
	}
	return false;
}


static bool down_slide_all_tiles(game_state_t* state) {
	// Iterate each column from bottom to top, sliding each tile as far downwards as we can
	bool found_tile_to_move = false;
	for (int32_t col_start = TILE_COLS - 1; col_start >= 0; col_start--) {
		// No need to try sliding the bottommost tile in a column
		uint32_t col_end = col_start + (TILES_PER_ROW * (TILE_COLS - 1));
		for (int32_t i = col_end - TILES_PER_ROW; i >= col_start; i -= TILES_PER_ROW) {
			if (slide_tile_down(state, i, col_end)) {
				found_tile_to_move = true;
			}
		}
	}
	return found_tile_to_move;
}

static void _spawn_random_piece(game_state_t* state) {
	// Build an array containing indexes of free tiles
	uint32_t free_tile_idxs[TILE_COUNT] = {0};
	uint32_t free_tile_count = 0;
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		game_tile_t* tile = &state->tiles[i];
		if (!tile->is_occupied) {
			free_tile_idxs[free_tile_count] = i;
			free_tile_count += 1;
		}
	}

	// Now, pick a random free tile to spawn the tile into
	uint32_t chosen_tile_idx = free_tile_idxs[rand() % free_tile_count];
	game_tile_t* chosen_tile = &state->tiles[chosen_tile_idx];

	// 10% chance to get a 4
	uint32_t piece_value = rand() % 10 == 1 ? 4 : 2;

	chosen_tile->is_occupied = true;
	chosen_tile->value = piece_value;

	float shrink_factor = 10.0;
	Rect to = _tile_rect_from_idx(state->view->content_layer_frame, chosen_tile_idx);
	Size from_size = size_make(
		to.size.width / shrink_factor,
		to.size.height / shrink_factor
	);
	Rect from = rect_make(
		point_make(
			rect_mid_x(to) - (from_size.width / 2.0),
			rect_mid_y(to) - (from_size.height / 2.0)
		),
		from_size
	);
	_enqueue_move_animation(state, chosen_tile_idx, 100, from);
}

static bool _is_game_over(game_state_t* state) {
	bool has_open_tile = false;
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		game_tile_t* tile = &state->tiles[i];
		if (!tile->is_occupied) {
			has_open_tile = true;
			break;
		}
	}
	if (has_open_tile) {
		return false;
	}
	return true;
}

static bool draw_in_progress_animations(game_state_t* state);

static void _start_new_game(game_state_t* state) {
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		game_tile_t* tile = &state->tiles[i];
		tile->is_occupied = false;
		tile->value = 0;
	}
	// Start off each game with two pieces on the board
	_spawn_random_piece(state);
	_spawn_random_piece(state);
	draw_in_progress_animations(state);
}

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

static bool draw_in_progress_animations(game_state_t* state) {
	// Have we finished all our animations?
	uint32_t now = ms_since_boot();
	bool finished_all = true;
	array_t* pending_animations = state->animation_state.pending_animations;
	for (uint32_t i = 0; i < pending_animations->size; i++) {
		tile_animation_t* anim = array_lookup(pending_animations, i);
		if (anim->base.end_time > now) {
			finished_all = false;
			break;
		}
	}
	if (finished_all) {
		state->animation_state.animations_ongoing = false;
		for (int32_t i = pending_animations->size - 1; i >= 0; i--) {
			tile_base_animation_t* anim = array_lookup(pending_animations, i);

			array_remove(pending_animations, i);
			free(anim);
		}

		draw_game_state(state);

		return false;
	}

	// Draw the tiles that are not a part of an active animation
	for (uint32_t i = 0; i < TILE_COUNT; i++) {
		if (!state->animation_state.tile_being_animated[i]) {
			_draw_tile(state, i);
		}
	}

	_draw_separators(state);

	for (uint32_t i = 0; i < pending_animations->size; i++) {
		tile_animation_t* anim = array_lookup(pending_animations, i);

		uint32_t elapsed_ms = now - anim->base.start_time;
		uint32_t duration_ms = anim->base.end_time - anim->base.start_time;
		float percent = elapsed_ms / (float)duration_ms;
		percent = min(1.0, percent);

		Rect r = rect_zero();
		if (anim->base.type == TILE_ANIMATION_TYPE_MOVE) {
			tile_move_animation_t* m = &anim->move;
			r = rect_make(
				point_make(
					lerp(rect_min_x(m->from), rect_min_x(m->to), percent),
					lerp(rect_min_y(m->from), rect_min_y(m->to), percent)
				),
				size_make(
					lerp(m->from.size.width, m->to.size.width, percent),
					lerp(m->from.size.height, m->to.size.height, percent)
				)
			);
		}

		_draw_tile_with_params(state, anim->base.draw_info, r);
	}

	// And kick off a timer to continue the animation
	gui_timer_start(16, (gui_timer_cb_t)draw_in_progress_animations, state);
	return true;
}

static void _key_entered(gui_elem_t* elem, uint32_t ch) {
	game_state_t* state = &state_s;

	if (state->animation_state.animations_ongoing) {
		printf("Dropping keypress because animations are ongoing: %c\n", ch);
		return;
	}

	_reset_turn_flags(state);
	memset(state->animation_state.tile_being_animated, 0, sizeof(state->animation_state.tile_being_animated));

	bool found_tile_to_move = false;
	if (ch == 'a' || ch == KEY_IDENT_LEFT_ARROW) {
		found_tile_to_move = left_slide_all_tiles(state);
	}
	else if (ch == 's' || ch == KEY_IDENT_RIGHT_ARROW) {
		found_tile_to_move = right_slide_all_tiles(state);
	}
	else if (ch == 'w' || ch == KEY_IDENT_UP_ARROW) {
		found_tile_to_move = up_slide_all_tiles(state);
	}
	else if (ch == 'r' || ch == KEY_IDENT_DOWN_ARROW) {
		found_tile_to_move = down_slide_all_tiles(state);
	}

	if (_is_game_over(state)) {
		printf("Game over! Resetting game state...\n");
		_start_new_game(state);
	}
	else {
		_spawn_random_piece(&state_s);
	}
	
#ifdef ANIMATIONS_ENABLED
	if (state->animation_state.animations_ongoing) {
		draw_in_progress_animations(state);
	}
	else {
		draw_game_state(state);
	}
#else
	draw_game_state(state);
#endif
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.2048");

	gui_window_t* window = gui_window_create("2048", 250, 250);
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
	// Maximum possible concurrent animations is the number of on-screen tiles
	state->animation_state.pending_animations = array_create(TILE_COUNT);

	srand(ms_since_boot());

	_start_new_game(state);

	draw_game_state(state);

	gui_enter_event_loop();

	return 0;
}
