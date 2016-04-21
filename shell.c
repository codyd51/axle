#include "shell.h"
#include "kernel.h"
#include "kb.h"

size_t CommandNum;
command_table_t CommandTable[MAX_COMMANDS];

int findCommand(char* command) {
	size_t i;
	int ret;

	for (i = 0; i < CommandNum + 1; i++) {
		ret = strcmp(command, CommandTable[i].name);

		if (ret == 0) return i;
	}
	terminal_writestring("\nCommand: ");
	terminal_writestring(command);
	terminal_writestring(" not found.");
	return -1;
}

void process_command(char* string) {
	//TODO split this up into its own stdlib function
	char* command = string, *arg = "", *c = string;
	while (*c) {
		if (*c == ' ') {
			*c = '\0'; arg = ++c;
		}
		else {
			c++;
		}
	}

	int i = findCommand(command);
	if (i >= 0) {
		//TODO better way to check if we have args
		//if they pass an argument, we assume the function they're calling accepts arguments
		if (strlen(arg) > 0) {
			void(*command_function)(void* arg) = CommandTable[i].function;
			command_function(arg);
		}
		else {
			void(*command_function)(void) = CommandTable[i].function;
			command_function();
		}
	}
}

void add_cursor() {
	terminal_putchar('_');
}

void remove_cursor() {
	terminal_removechar();
}

void process_character(char* inputstr, char ch) {
	//handle backspace
	if (ch == '\b') {
		//remove last character from input string
		if (strlen(inputstr) > 0) {
			inputstr = delchar(inputstr);

			//remove cursor
			remove_cursor();
			//remove character
			terminal_removechar();

			//add in cursor again
			add_cursor();
		}
	}
	//handle newline
	else if (ch == '\n') {

	}
	else  {
		//add this character to the input string and output it
		strccat(inputstr, ch);

		//remove cursor character
		remove_cursor();
		terminal_putchar(ch);
		add_cursor();
	}
}

char* get_inputstring() {
	char* input = malloc(sizeof(char) * 256);
	unsigned char c = 0;
	do {
		c = getchar();
		process_character(input, c);
	} while (c != '\n');
	//remove cursor character
	remove_cursor();
	return input;
}

void shell() {
	terminal_writestring("\naxle> ");

	char* input = get_inputstring();
	process_command(input);
}

void add_new_command(char* name, char* description, void* function) {
	if (CommandNum + 1 < MAX_COMMANDS) {
		CommandTable[CommandNum].name = name;
		CommandTable[CommandNum].description = description;
		CommandTable[CommandNum].function = function;

		CommandNum++;
	}
	return;
}

void help_command() {
	terminal_writestring("\nAXLE Shell v0.0.1");
	terminal_writestring("\nAll commands listed here are internally defined.");
	terminal_writestring("\nType 'help' to see this list\n");
	for (size_t i = 0; i < CommandNum; i++) {
		terminal_writestring("\n\t");
		terminal_writestring(CommandTable[i].name);
		terminal_writestring("\t");
		terminal_writestring(CommandTable[i].description);
	}
}

void echo_command(char* arg) {
	terminal_writestring("\n");
	terminal_writestring(arg);
}

void empty_command() {
	//do nothing if nothing was entered
}

void init_shell() {
	add_new_command("help", "Display help information", help_command);
	add_new_command("echo", "Outputs args to stdout", echo_command);
	add_new_command("", "", empty_command);
}
