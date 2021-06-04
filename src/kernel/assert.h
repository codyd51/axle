#ifndef ASSERT_H
#define ASSERT_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/interrupts/idt_structures.h>

#define NotImplemented() do {_panic("Not implemented", __FILE__, __LINE__);} while(0);
#define Deprecated() do {_panic("Explicitly deprecated", __FILE__, __LINE__);} while(0);

#define panic(msg) _panic(msg, __FILE__, __LINE__);
#define PANIC(msg) panic(msg)

#define assert(cond, msg) if (!(cond)) {PANIC(msg)}
#define ASSERT(cond, msg, ...) assert(cond, msg)

void _panic(const char* msg, const char* file, int line);

void task_assert(bool cond, const char* msg, const register_state_t* regs);

#endif
