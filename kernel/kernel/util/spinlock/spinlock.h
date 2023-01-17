#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

union x86_rflags {
    uint64_t raw;
    struct {
        uint32_t carry_flag:1,		/* Last math op resulted carry */
        __reserved1_0:1,	/* Always 1 */
        parity_flag:1,		/* Last op resulted even parity */
        __reserved0_0:1,	/* Must be zero */
        auxiliary_flag:1,	/* BCD carry */
        __reserved0_1:1,	/* Must be zero */
        zero_flag:1,		/* Last op resulted zero */
        sign_flag:1,		/* Last op resulted negative */
        trap_flag:1,		/* Enable single-step mode */
        irqs_enabled:1,		/* Maskable interrupts enabled? */
        direction_flag:1,	/* String ops direction */
        overflow_flag:1,	/* Last op MSB overflowed */
        io_privilege:2,		/* IOPL of current task */
        nested_task:1,		/* Controls chaining of tasks */
        __reserved0_2:1,	/* Must be zero */
        resume_flag:1,		/* Debug exceptions related */
        virtual_8086:1,		/* Enable/disable 8086 mode */
        alignment_check:1,	/* Enable alignment checking? */
        virtual:2,		/* Virtualization fields */
        id_flag:1,		/* CPUID supported if writable */
        __reserved0_3:10;	/* Must be zero */
        uint32_t __reserved0_4;		/* Must be zero */
    } __packed;
};

typedef struct spinlock_t {
	uint32_t flag;
	char* name;
	bool interrupts_enabled_before_acquire;
	int owner_pid;
	int nest_count;
    union x86_rflags rflags;
} spinlock_t;

void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

#endif
