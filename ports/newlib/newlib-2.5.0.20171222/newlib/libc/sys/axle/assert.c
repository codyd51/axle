#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include <kernel/amc.h>

void task_assert(bool cond, const char* msg);

void assert(bool cond, const char* msg) {
	task_assert(cond, msg);
}
