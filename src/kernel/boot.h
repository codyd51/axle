#ifndef BOOT_EXPORTS_H
#define BOOT_EXPORTS_H

#include <stdint.h>

//labels defined in boot.s
extern uint32_t _kernel_stack_bottom;
extern uint32_t _kernel_stack_top;

//labels defined in link.ld
extern uint32_t _kernel_image_start;
extern uint32_t _kernel_image_end;

#endif