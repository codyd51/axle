#ifndef KBMAN_H
#define KBMAN_H

#include <stdbool.h>
#include <stdint.h>

#define KEY_UP 		72
#define KEY_DOWN 	80
#define KEY_LEFT 	75
#define KEY_RIGHT 	77

void kbman_process(char c);
void kbman_process_release(char c);
bool key_down(char c);

#endif
