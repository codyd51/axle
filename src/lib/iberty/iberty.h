#include <std/std.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef EOS
#define EOS '\0'
#endif

#define INITIAL_MAXARGC 8

void freeargv (char **vector);
char **buildargv (const char *input, int *ac);
