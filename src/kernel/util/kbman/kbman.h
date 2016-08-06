#ifndef KBMAN_H
#define KBMAN_H

#include <stdbool.h>

#define KEY_UP 		0x48
#define KEY_DOWN 	0x50
#define KEY_LEFT 	0x4B
#define KEY_RIGHT 	0x4D

void kbman_process(char c);
void kbman_process_release(char c);
bool key_down(char c);

#endif
