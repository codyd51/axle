#include <stdio.h>
#include <stdlib.h>

#include "assert.h"

void assert(bool cond, const char* msg) {
	if (!cond) {
		printf("Assertion failed: %s\n", msg);
		exit(1);
	}
}
