#ifndef IDT_H
#define IDT_H

// IDT vectors axle maps IRQs to
#define INT_VECOR_IRQ0   32
#define INT_VECTOR_IRQ1  33
#define INT_VECTOR_IRQ2  34
#define INT_VECTOR_IRQ3  35
#define INT_VECTOR_IRQ4  36
#define INT_VECTOR_IRQ5  37
#define INT_VECTOR_IRQ6  38
#define INT_VECTOR_IRQ7  39
#define INT_VECTOR_IRQ8  40
#define INT_VECTOR_IRQ9  41
#define INT_VECTOR_IRQ10 42
#define INT_VECTOR_IRQ11 43
#define INT_VECTOR_IRQ12 44
#define INT_VECTOR_IRQ13 45
#define INT_VECTOR_IRQ14 46
#define INT_VECTOR_IRQ15 47

#define INT_VECTOR_IRQ127  127
#define INT_VECTOR_SYSCALL INT_VECTOR_IRQ127

void idt_init(void);

#endif
