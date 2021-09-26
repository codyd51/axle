#ifndef SYSCALL_H
#define SYSCALL_H

#include <std/std.h>

void syscall_init();
bool syscall_is_setup();
void syscall_add(void* syscall);

#define DECL_SYSCALL(fn, ...) int sys_##fn(__VA_ARGS__)

#define _ASM_SYSCALL_ARGS_0()
#define _ASM_SYSCALL_ARGS_1(P1) P1 p1
#define _ASM_SYSCALL_ARGS_2(P1, P2) _ASM_SYSCALL_ARGS_1(P1), P2 p2
#define _ASM_SYSCALL_ARGS_3(P1, P2, P3) _ASM_SYSCALL_ARGS_2(P1, P2), P3 p3
#define _ASM_SYSCALL_ARGS_4(P1, P2, P3, P4) _ASM_SYSCALL_ARGS_3(P1, P2, P3), P4 p4
#define _ASM_SYSCALL_ARGS_5(P1, P2, P3, P4, P5) _ASM_SYSCALL_ARGS_4(P1, P2, P3, P4), P5 p5

#define _ASM_SYSCALL_BODY_0(num) "int $0x80" : "=a" (a) : "0" (num)
#define _ASM_SYSCALL_BODY_1(num) _ASM_SYSCALL_BODY_0(num), "b" ((int)p1)
#define _ASM_SYSCALL_BODY_2(num) _ASM_SYSCALL_BODY_1(num), "c" ((int)p2)
#define _ASM_SYSCALL_BODY_3(num) _ASM_SYSCALL_BODY_2(num), "d" ((int)p3)
#define _ASM_SYSCALL_BODY_4(num) _ASM_SYSCALL_BODY_3(num), "S" ((int)p4)
#define _ASM_SYSCALL_BODY_5(num) _ASM_SYSCALL_BODY_4(num), "D" ((int)p5)

#define _ARG_COUNT(z, a, b, c, d, e, f, N, ...) N
#define ARG_COUNT(...) _ARG_COUNT(,##__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)

#define __DEFN_SYSCALL(N, fn, num, ...) \
int sys_##fn(_ASM_SYSCALL_ARGS_##N(__VA_ARGS__)) { \
	int a; asm volatile(_ASM_SYSCALL_BODY_##N(num)); return a; \
}
#define _DEFN_SYSCALL(N, fn, num, ...) __DEFN_SYSCALL(N, fn, num, ##__VA_ARGS__)
#define DEFN_SYSCALL(fn, num, ...) _DEFN_SYSCALL(ARG_COUNT(__VA_ARGS__), fn, num, ##__VA_ARGS__)

#endif
