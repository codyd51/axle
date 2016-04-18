#define MAX_COMMANDS 100

typedef struct {
	char* name;
	char* description;
	void* function;
} command_table_t;

extern void add_new_command();
extern void help_command();
extern void empty_command();
extern void terminal_putchar(char c);
extern void terminal_writestring(const char* data);