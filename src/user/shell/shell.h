#include <std/std.h>

#define MAX_COMMANDS 100

typedef struct {
	char* name;
	char* description;
	void* function;
} command_table_t;

void add_new_command(char* name, char* description, void(*function)());
void shell_init();
int shell();
