#ifndef ASSERT_H
#define ASSERT_H

#include <stdint.h>

#define NotImplemented() do {_assert("Not implemented", __FILE__, __LINE__);} while(0);
#define assert(msg) do {_assert(msg, __FILE__, __LINE__);} while(0);

void _assert(const char* msg, const char* file, int line);

#endif