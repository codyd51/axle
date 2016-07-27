#ifndef SNAKE_H
#define SNAKE_H

#include <stdint.h>

typedef struct snake_player {
	uint8_t length;
	int is_alive;
} snake_player_t;

typedef struct game_state {
	snake_player_t* player;
	char last_move;
	uint8_t last_head_x;
	uint8_t last_head_y;
	int is_running;
} game_state_t;

void play_snake(void);

#endif
