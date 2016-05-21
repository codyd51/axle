#include <std/std.h>

#define MAX_COMMANDS 100
#ifndef NULL
#define NULL 0
#endif

#ifndef EOS
#define EOS '\0'
#endif

#define INITIAL_MAXARGC 8

typedef struct {
	char* name;
	char* description;
	void* function;
} command_table_t;

void add_new_command(char* name, char* description, void* function);
void init_shell();
int shell();
