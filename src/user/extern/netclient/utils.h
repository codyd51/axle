#ifndef CSS_H
#define CSS_H

#include <stdint.h>
#include "shims.h"

bool str_is_whitespace(const char* s);
bool str_ends_with(const char* s, uint32_t s_len, const char* t, uint32_t t_len);
int strcicmp(char const* a, char const* b);

#endif