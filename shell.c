#include "shell.h"
#include "kernel.h"
#include "kb.h"

size_t CommandNum;
command_table_t CommandTable[MAX_COMMANDS];
char* input_string;

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
	int i = findCommand(string);
	if (i >= 0) {
		void(*command_function)(void) = CommandTable[i].function;
		command_function();
	}
}

void shell() {
	terminal_writestring("\nprompt> ");

	char* input = get_input();
	terminal_writestring("\ngot input: ");
	terminal_writestring(input);
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
	terminal_writestring("\nType 'help' to see this list");
	for (size_t i = 0; i < CommandNum; i++) {
		terminal_writestring("\n\n\t");
		terminal_writestring(CommandTable[i].name);
		terminal_writestring("\t");
		terminal_writestring(CommandTable[i].description);
	}
}

void empty_command() {
	//do nothing if nothing was entered
}

void init_shell() {
	add_new_command("help", "Display help information", help_command);
	add_new_command("", "", empty_command);
}
