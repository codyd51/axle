#ifndef STD_PANIC_H
#define STD_PANIC_H

#include "std_base.h"
#include <stdint.h>

__BEGIN_DECLS

#define PANIC(msg) panic_msg(msg, __LINE__, __FILE__);
#define ASSERT(x, msg) if (!(x)) PANIC(msg)

STDAPI void panic_msg(const char* msg, uint16_t line, const char* file);

__END_DECLS

#endif // STD_PANIC_H
