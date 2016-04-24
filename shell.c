#include "shell.h"
#include "kernel.h"
#include "kb.h"

size_t CommandNum;
command_table_t CommandTable[MAX_COMMANDS];

int findCommand(char* command, int numArgs) {
	size_t i;
	int ret;

	for (i = 0; i < CommandNum + 1; i++) {
		ret = strcmp(command, CommandTable[i].name);

		if (ret == 0) {
			//ensure the method they're calling has the same number of args as what they passed
			if (CommandTable[i].numArgs == numArgs) {
				return i;
			}
			else {
				//buffer for converting integers to strings
				char b[6];

				terminal_writestring("\nExpected ");

				itoa(CommandTable[i].numArgs, b);
				terminal_writestring(b);

				terminal_writestring(" arguments to command ");
				terminal_writestring(CommandTable[i].name);
				terminal_writestring(", received ");

				itoa(numArgs, b);
				terminal_writestring(b);

				terminal_writestring(".");
				return -1;
			}
		}
	}
	terminal_writestring("\nCommand: ");
	terminal_writestring(command);
	terminal_writestring(" not found.");
	return -1;
}

void process_command(char* string) {
	//TODO split this up into its own stdlib function
	char* command = string, *arg1 = "", *arg2 = "", arg3 = "", arg4 = "", arg5 = "", *c = string;
	while (*c) {
		if (*c == ' ') {
			*c = '\0'; 

			//fill this token into the next free arg
			if (strlen(arg1) == 0) {
				arg1 = ++c;
			}
			else if (strlen(arg2) == 0) {
				arg2 = ++c;
			}
			else if (strlen(arg3) == 0) {
				arg3 = ++c;
			}
			else if (strlen(arg4) == 0) {
				arg4 = ++c;
			}
			else {
				arg5 = ++c;
			}
		}
		else {
			c++;
		}
	}

	//without proper array support in C, I don't have a good way to find how many of the above are filled without, checking if they contain data
	int numArgs = 0;
	//there will always be at least 1 character for the newline character, so we ignore this
	if (strlen(arg1) > 1) numArgs = 1;
	if (strlen(arg2) > 1) numArgs = 2;
	if (strlen(arg3) > 1) numArgs = 3;
	if (strlen(arg4) > 1) numArgs = 4;
	if (strlen(arg5) > 1) numArgs = 5;

	int i = findCommand(command, numArgs);
	if (i >= 0) {
		//I would use a switch here, but switch cases do not scope as you'd expect them to,
		//so the compiler complains that I'm redefining the function. This is slightly more maintainable, IMHO
		if (numArgs == 0) {
			void(*command_function)() = CommandTable[i].function;
			command_function();
		}
		else if (numArgs == 1) {
			void(*command_function)(void* arg1) = CommandTable[i].function;
			command_function(arg1);
		}
		else if (numArgs == 2) {
			void(*command_function)(void* arg1, void* arg2) = CommandTable[i].function;
			command_function(arg1, arg2);
		}
		else if (numArgs == 3) {
			void(*command_function)(void* arg1, void* arg2, void* arg3) = CommandTable[i].function;
			command_function(arg1, arg2, arg3);
		}
		else if (numArgs == 4) {
			void(*command_function)(void* arg1, void* arg2, void* arg3, void* arg4) = CommandTable[i].function;
			command_function(arg1, arg2, arg3, arg4);
		}
		else {
			void(*command_function)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5) = CommandTable[i].function;
			command_function(arg1, arg2, arg3, arg4, arg5);
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

void add_new_command(char* name, char* description, void* function, int numArgs) {
	if (CommandNum + 1 < MAX_COMMANDS) {
		CommandTable[CommandNum].name = name;
		CommandTable[CommandNum].description = description;
		CommandTable[CommandNum].function = function;
		CommandTable[CommandNum].numArgs = numArgs;

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
	add_new_command("help", "Display help information", help_command, 0);
	add_new_command("echo", "Outputs args to stdout", echo_command, 1);
	add_new_command("", "", empty_command, 0);
}



