#ifndef PANIC_H
#define PANIC_H

#define PANIC(msg) panic_msg(msg, __LINE__, __FILE__);
#define ASSERT(x, msg) if (!(x)) PANIC(msg)

#endif
