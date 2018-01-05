#ifndef ASSERT_H
#define ASSERT_H

#include <stdint.h>

#define NotImplemented() do {_panic("Not implemented", __FILE__, __LINE__);} while(0);

#define panic(msg) do {_panic(msg, __FILE__, __LINE__);} while (0);
#define PANIC(msg) panic(msg)

#define assert(cond, msg) if (!(cond)) {PANIC(msg)}
#define ASSERT(cond, msg, ...) assert(cond, msg)

void _panic(const char* msg, const char* file, int line);

#endif