#include "snake.h"
#include <kernel/kernel.h>
#include <std/kheap.h>

#define BOARD_SIZE VGA_WIDTH * VGA_HEIGHT

//u16int* screen_buffer[VGA_WIDTH][VGA_HEIGHT] = (u16int*)0x8B000;

void board_clear() {
	for (int y = 0; y < VGA_HEIGHT; y++) {
		for (int x = 0; x < VGA_WIDTH; x++) {
			terminal_putentryat(" ", 68, x, y);
		}
	}
}

void move_up(game_state_t* game_state) {
	int new_head_x = game_state->last_head_x;
	int new_head_y = game_state->last_head_y - 1;

	game_state->last_head_x = new_head_x;
	game_state->last_head_y = new_head_y;

	terminal_putentryat("@", 66, new_head_x, new_head_y);
}

void move_down(game_state_t* game_state) {
	int new_head_x = game_state->last_head_x;
	int new_head_y = game_state->last_head_y + 1;

	game_state->last_head_x = new_head_x;
	game_state->last_head_y = new_head_y;

	terminal_putentryat("@", 66, new_head_x, new_head_y);
}

void move_left(game_state_t* game_state) {
	int new_head_x = game_state->last_head_x - 1;
	int new_head_y = game_state->last_head_y;

	game_state->last_head_x = new_head_x;
	game_state->last_head_y = new_head_y;

	terminal_putentryat("@", 66, new_head_x, new_head_y);
}

void move_right(game_state_t* game_state) {
	int new_head_x = game_state->last_head_x + 1;
	int new_head_y = game_state->last_head_y;

	game_state->last_head_x = new_head_x;
	game_state->last_head_y = new_head_y;

	terminal_putentryat("@", 66, new_head_x, new_head_y);
}

void move_dispatch(game_state_t* game_state) {
	switch (game_state->last_move) {
		case 'w':
			move_up(game_state);
			break;
		case 'a':
			move_left(game_state);
			break;
		case 's':
			move_down(game_state);
			break;
		case 'd':
			move_right(game_state);
			break;
	}
}

void game_tick(game_state_t* game_state) {
	static int bufPos;

	if (game_state->player->is_alive != 0) {
		game_state->is_running = 1;

		board_clear();
	
		int y = bufPos / VGA_WIDTH;
		int x = bufPos - (y * VGA_WIDTH);

		move_dispatch(game_state);

		sleep(100);

		if (haskey()) {
			char ch = getchar();
			if (ch == 'w' || ch == 'a' || ch == 's' || ch == 'd') {
				game_state->last_move = ch;
			}
		}

		bufPos++;
	}
	else {
		//end game
		game_state->is_running = 0;
	}
}

void play_snake() {
	terminal_clear();

	//while quit game character is not matched
	char ch;

	game_state_t* game_state = kmalloc(sizeof(game_state_t));
	snake_player_t* player = kmalloc(sizeof(snake_player_t));
	game_state->player->is_alive = 1;
	game_state->last_move = 'd';
	player->length = 10;
	
	board_clear();

	//ensure game_tick runs at least once
	do {
		game_tick(game_state);
	} while (game_state->is_running != 0);
	
	printf_info("Thanks for playing!");
}
