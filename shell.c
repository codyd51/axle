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
	char* command = string;
	char* c = string;

	//maximum 10 arguments of 30 characters each
	char args[10][30];
	char* argbuffer = "";
	int currArg = 0;
	while (*c) {
		if (*c == ' ') {
			*c = '\0'; 

			strcpy(args[currArg], ++c);
			currArg++;
		}
		else {
			c++;
		}
	}

	int i = findCommand(command, currArg);
	if (i >= 0) {
		if (currArg == 0) {
			void(*command_function)() = CommandTable[i].function;
			command_function();
		}
		else {
			void(*command_function)(void* arg1, ...) = CommandTable[i].function;
			//there's no real elegant way to pass a variadic number of args to a function in c
			//we really just have to manually call the function with every possible number of args
			switch (currArg) {
			case 1:
				command_function(args[0]);
				break;
			case 2:
				command_function(args[0], args[1]);
				break;
			case 3:
				command_function(args[0], args[1], args[2]);
				break;
			case 4:
				command_function(args[0], args[1], args[2], args[3]);
				break;
			case 5:
				command_function(args[0], args[1], args[2], args[3], args[4]);
				break;
			case 6:
				command_function(args[0], args[1], args[2], args[3], args[4], args[5]);
				break;
			case 7:
				command_function(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
				break;
			case 8:
				command_function(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
				break;
			case 9:
				command_function(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
				break;
			case 10:
			default:
				command_function(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
			}
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

void echo2_command(char* arg1, char* arg2) {
	terminal_writestring("\n");
	terminal_writestring(arg1);
	terminal_writestring(arg2);
}

void empty_command() {
	//do nothing if nothing was entered
}

void init_shell() {
	add_new_command("help", "Display help information", help_command, 0);
	add_new_command("echo", "Outputs args to stdout", echo_command, 1);
	add_new_command("echo2", "Outputs 2 args to stdout", echo2_command, 2);
	add_new_command("", "", empty_command, 0);
}



