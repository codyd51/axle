#ifndef BIG_INT_H
#define BIG_INT_H

#include <stdint.h>

typedef struct big_int big_int_t;

big_int_t* big_int_alloc();
void big_int_or(big_int_t* bi, uint32_t val);

#endif
