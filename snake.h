#ifndef SNAKE_H
#define SNAKE_H

#include "common.h"

typedef struct snake_player {
	u8int length;
	int is_alive;
} snake_player_t;

typedef struct game_state {
	snake_player_t* player;
	char last_move;
	u8int last_head_x;
	u8int last_head_y;
	int is_running;
} game_state_t;

void play_snake();

#endif
