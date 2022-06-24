#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_remap(int offset1, int offset2);
void pic_signal_end_of_interrupt(uint8_t irq_no);
void pic_set_interrupt_enabled(int interrupt, bool enabled);
bool is_interrupt_vector_delivered_by_pic(int interrupt);

#endif
