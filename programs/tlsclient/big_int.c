#include <stdint.h>
#include <stdlib.h>
#include "big_int.h"

typedef struct big_int {
    uint32_t* components;
    uint32_t component_count;
    uint32_t max_component_count;
} big_int_t;

big_int_t* big_int_alloc() {
    big_int_t* bi = calloc(1, sizeof(big_int_t));
    bi->max_component_count = 4;
    bi->component_count = 0;
}

void big_int_or(big_int_t* bi, uint32_t val) {

}