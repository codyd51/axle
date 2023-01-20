#ifndef IDT_H
#define IDT_H

// CPU interrupt vectors
#define INT_VECTOR_INT0  0
#define INT_VECTOR_INT1  1
#define INT_VECTOR_INT2  2
#define INT_VECTOR_INT3  3
#define INT_VECTOR_INT4  4
#define INT_VECTOR_INT5  5
#define INT_VECTOR_INT6  6
#define INT_VECTOR_INT7  7
#define INT_VECTOR_INT8  8
#define INT_VECTOR_INT9  9
#define INT_VECTOR_INT10 10
#define INT_VECTOR_INT11 11
#define INT_VECTOR_INT12 12
#define INT_VECTOR_INT13 13
#define INT_VECTOR_INT14 14

// PIC interrupts are mapped to IDT vectors 32+
#define INT_VECTOR_PIC_0  32
#define INT_VECTOR_PIC_1  33
#define INT_VECTOR_PIC_2  34
#define INT_VECTOR_PIC_3  35
#define INT_VECTOR_PIC_4  36
#define INT_VECTOR_PIC_5  37
#define INT_VECTOR_PIC_6  38
#define INT_VECTOR_PIC_7  39
#define INT_VECTOR_PIC_8  40
#define INT_VECTOR_PIC_9  41
#define INT_VECTOR_PIC_10 42
#define INT_VECTOR_PIC_11 43
#define INT_VECTOR_PIC_12 44
#define INT_VECTOR_PIC_13 45
#define INT_VECTOR_PIC_14 46
#define INT_VECTOR_PIC_15 47

// APIC interrupts are mapped to IDT vectors 64+
#define INT_VECTOR_APIC_0  64
#define INT_VECTOR_APIC_1  65
#define INT_VECTOR_APIC_2  66
#define INT_VECTOR_APIC_3  67
#define INT_VECTOR_APIC_4  68
#define INT_VECTOR_APIC_5  69
#define INT_VECTOR_APIC_6  70
#define INT_VECTOR_APIC_7  71
#define INT_VECTOR_APIC_8  72
#define INT_VECTOR_APIC_9  73
#define INT_VECTOR_APIC_10 74
#define INT_VECTOR_APIC_11 75
#define INT_VECTOR_APIC_12 76
#define INT_VECTOR_APIC_13 77
#define INT_VECTOR_APIC_14 78
#define INT_VECTOR_APIC_15 79

#define INT_VECTOR_IRQ128 128
#define INT_VECTOR_SYSCALL INT_VECTOR_IRQ128

void idt_init(void);

#endif
