#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

int execve(const char *filename, char *const argv[], char *const envp[]);

#endif
