#include "snake.h"
#include <kernel/kernel.h>
#include <std/kheap.h>
#include <std/printf.h>
#include <kernel/drivers/terminal/terminal.h>

#define BOARD_SIZE TERM_AREA

//u16int* screen_buffer[VGA_WIDTH][VGA_HEIGHT] = (u16int*)0x8B000;

static void draw_head(game_state_t* game_state) {
	terminal_movecursor((term_cursor){
		game_state->last_head_x,
		game_state->last_head_y
	});
	terminal_putchar('@');
}

void move_up(game_state_t* game_state) {
	--game_state->last_head_y;
	draw_head(game_state);
}

void move_down(game_state_t* game_state) {
	++game_state->last_head_y;
	draw_head(game_state);
}

void move_left(game_state_t* game_state) {
	--game_state->last_head_x;
	draw_head(game_state);
}

void move_right(game_state_t* game_state) {
	++game_state->last_head_x;
	draw_head(game_state);
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
	if (game_state->player->is_alive != 0) {
		game_state->is_running = 1;

		terminal_clear();

		move_dispatch(game_state);

		sleep(100);

		if (haskey()) {
			char ch = getchar();
			if (ch == 'w' || ch == 'a' || ch == 's' || ch == 'd') {
				game_state->last_move = ch;
			}
		}
	}
	else {
		//end game
		game_state->is_running = 0;
	}
}

void play_snake(void) {
	game_state_t* game_state = (game_state_t*)kmalloc(sizeof(game_state_t));
	snake_player_t* player = (snake_player_t*)kmalloc(sizeof(snake_player_t));
	game_state->player->is_alive = 1;
	game_state->last_move = 'd';
	player->length = 10;
	
	terminal_clear();

	//ensure game_tick runs at least once
	do {
		game_tick(game_state);
	} while (game_state->is_running != 0);
	
	printf_info("Thanks for playing!");
}
