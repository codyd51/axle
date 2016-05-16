#ifndef PANIC_H
#define PANIC_H

#define ASSERT(x) if (!(x)) panic(__LINE__, __FILE__);
#define PANIC(msg) panic_msg(msg, __LINE__, __FILE__);

#endif
