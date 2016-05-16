#include "shell.h"
#include <kernel/kernel.h>
#include <kernel/drivers/kb/kb.h>
#include <user/shell/programs/asmjit/asmjit.h>
#include <kernel/drivers/pit/timer.h>
#include <user/shell/programs/snake/snake.h>

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
				printf("Expected %d arguments to command %s, received %d.", CommandTable[i].numArgs, CommandTable[i].name, numArgs);
				return -1;
			}
		}
	}
	printf("Command %s not found.", command);
	return -1;
}

void prepare_shell() {
	printf("\n");
}

void process_command(char* string) {
	prepare_shell();

	//the command name will be the first string-seperated token in the string
	char* command = string_split(string, ' ', 0);

	//maximum 10 arguments of 30 characters each
	static char args[10][30];
	//we start looking for arguments at the second token
	//since the first token contains the command name
	int tokindex = 1;
	while (string_split(string, ' ', tokindex)) {
		strcpy(args[tokindex-1], string_split(string, ' ', tokindex));

		tokindex++;
	}

	//tokindex was increased by 1 to offset the actual command name
	int argcount = tokindex - 1;

	int i = findCommand(command, argcount);
	if (i >= 0) {
		if (argcount == 0) {
			void(*command_function)() = CommandTable[i].function;
			command_function();
		}
		else {
			void(*command_function)(void* arg1, ...) = CommandTable[i].function;
			//there's no real elegant way to pass a variadic number of args to a function in c
			//we really just have to manually call the function with every possible number of args
			switch (argcount) {
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

void process_character(char* inputstr, char ch) {
	//handle backspace
	if (ch == '\b') {
		//remove last character from input string
		if (strlen(inputstr) > 0) {
			char lastChar = inputstr[strlen(inputstr)-1];
			//if we're removing a space, check if we should reset the color to indicate a command
			if (lastChar == ' ') {
				//TODO make sure there's only one word left before resetting color
				terminal_settextcolor(COLOR_LIGHT_BLUE);
			}

			inputstr = delchar(inputstr);
			terminal_removechar();
		}
	}
	//handle newline
	else if (ch == '\n') {
		
	}
	else  {
		//add this character to the input string and output it
		strccat(inputstr, ch);
		terminal_putchar(ch);
	}

	//if this character was a space, change text color to indicate an argument
	if (ch == ' ') {
		terminal_settextcolor(COLOR_LIGHT_MAGENTA);
	}
}

char* get_inputstring() {
	char* input = kmalloc(sizeof(char) * 256);
	unsigned char c = 0;
	do {
		c = getchar();
		process_character(input, c);
	} while (c != '\n');

	return input;
}

int shell() {
	//reset terminal color in case it was changed
	terminal_settextcolor(COLOR_GREEN);
	printf("\naxle> ");

	//set terminal color to input color
	terminal_settextcolor(COLOR_LIGHT_BLUE);
	
	char* input = get_inputstring();

	//set terminal color to stdout color
	terminal_settextcolor(COLOR_WHITE);
	process_command(input);

	if (strcmp(input, "shutdown") == 0) {
		return 1;
	}
	return 0;
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

//defined in kernel.c
extern void print_os_name();
void help_command() {
	print_os_name();
	terminal_settextcolor(COLOR_WHITE);
	
	printf("\nAll commands listed here are internally defined.");
	printf("\nType 'help' to see this list\n");
	for (size_t i = 0; i < CommandNum; i++) {
		printf("\n\t%s\t%s", CommandTable[i].name, CommandTable[i].description);
	}
}

void echo_command(char* arg) {
	printf("%s", arg);
}

void echo2_command(char* arg1, char* arg2) {
	printf("%s %s", arg1, arg2);
}

void time_command() {
	printf("%d", time());
}

void date_command() {
	printf(date());
}

void empty_command() {
	//do nothing if nothing was entered
}

void clear_command() {
	terminal_clear();
}

void asmjit_command() {
//	asmjit();
}

void tick_command() {
	printf("%d", tickCount());
}

void snake_command() {
	play_snake();
}

void init_shell() {
	//set shell color
	terminal_settextcolor(COLOR_GREEN);
	
	//set up command table
	add_new_command("help", "Display help information", help_command, 0);
	add_new_command("echo", "Outputs args to stdout", echo_command, 1);
	add_new_command("echo2", "Outputs 2 args to stdout", echo2_command, 2);
	add_new_command("time", "Outputs system time", time_command, 0);
	add_new_command("date", "Outputs system time as date format", date_command, 0);
	add_new_command("clear", "Clear terminal", clear_command, 0);
	add_new_command("asmjit", "Starts JIT prompt", asmjit_command, 0);
	add_new_command("tick", "Prints current tick count from PIC", tick_command, 0);
	add_new_command("snake", "Have some fun!", snake_command, 0);
	add_new_command("", "", empty_command, 0);
}



