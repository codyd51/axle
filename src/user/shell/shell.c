#include "shell.h"
#include <kernel/kernel.h>
#include <kernel/drivers/kb/kb.h>
#include <user/shell/programs/asmjit/asmjit.h>
#include <kernel/drivers/pit/pit.h>
#include <user/shell/programs/snake/snake.h>
#include <tests/gfx_test.h>
#include <std/kheap.h>
#include <std/memory.h>
#include <lib/iberty/iberty.h>
#include <tests/test.h>
#include <std/printf.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/vesa/vesa.h>
#include <gfx/lib/gfx.h>

size_t CommandNum;
command_table_t CommandTable[MAX_COMMANDS];

int findCommand(char* command) {
	size_t i;
	int ret;

	for (i = 0; i < CommandNum + 1; i++) {
		ret = strcmp(command, CommandTable[i].name);

		if (ret == 0) {
			return i;
		}
	}
	printf("Command '\e[9;%s\e[15;' not found.", command);
	return -1;
}

void prepare_shell() {
	printf("\n");
}

void process_command(char* string) {
	prepare_shell();

	if ((string != NULL) && (string[0] == '\0'))
		return;

	int argc;
	char *sdup = strdup(string);
	char **argv = buildargv(sdup, &argc);

	//split the string to get the command, the rest is taken care of by libiberty
	size_t out;
	char** tokens = strsplit(sdup, " ", &out);
	kfree(sdup);

	if (out == 0)
		return;

	char* command = tokens[0];

	int i = findCommand(command);
	if (i >= 0) {
		void (*command_function)(int, char **) = (void(*)(int, char**))CommandTable[i].function;
		command_function(argc, argv);
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
				printf("\e[9;");
			}

			inputstr = delchar(inputstr);
			//terminal driver will remove last char
			terminal_putchar(ch);
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
		printf("\e[13;");
	}
}

char* get_inputstring() {
	char* input = (char*)kmalloc(sizeof(char) * 256);
	unsigned char c = 0;
	do {
		c = getchar();
		process_character(input, c);
	} while (c != '\n');

	return input;
}

int shell() {
	//reset terminal color in case it was changed
	//then set to input color
	printf("\n\e[10;axle> \e[9;");
	
	char* input = get_inputstring();

	//set terminal color to stdout color
	printf("\e[15;");
	process_command(input);
	//kfree(input);

	if (strcmp(input, "shutdown") == 0) {
		return 1;
	}
	return 0;
}

void add_new_command(char* name, char* description, void(*function)(void)) {
	if (CommandNum + 1 < MAX_COMMANDS) {
		CommandTable[CommandNum].name = name;
		CommandTable[CommandNum].description = description;
		CommandTable[CommandNum].function = (void*)function;

		CommandNum++;
	}
	return;
}

//defined in kernel.c
extern void print_os_name();
void help_command() {
	print_os_name();
	printf("\e[15;");
	printf("\nAll commands listed here are internally defined.");
	printf("\nType 'help' to see this list\n");
	for (size_t i = 0; i < CommandNum; i++) {
		int spaces_needed = 10 - strlen(CommandTable[i].name);
		printf("\n\t%s", CommandTable[i].name);
		for (int i = 0; i < spaces_needed; i++) {
			printf(" ");
		}
		printf("%s", CommandTable[i].description);
	}
}

void echo_command(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		printf("%s%s", argv[i], i == argc ? "" : " ");
	}
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
	asmjit();
}

void tick_command() {
	printf("%d", time());
}

void snake_command() {
	play_snake();
}

void shutdown_command() {

}

void startx_command() {
	printf_info("Press 'q' to exit");
	sleep(500);

	//switch into VGA for boot screen
	Screen* vga_screen = switch_to_vga();
	
	//display boot screen
	vga_boot_screen(vga_screen);

	gfx_teardown(vga_screen);
	switch_to_text();

	//switch to VESA for x serv
	Screen* vesa_screen = switch_to_vesa();
	test_xserv(vesa_screen);
}

void shell_init() {
	//set shell color
	printf("\e[10;");
	
	//set up command table
	add_new_command("help", "Display help information", help_command);
	add_new_command("echo", "Outputs args to stdout", (void(*)())echo_command);
	add_new_command("time", "Outputs system time", time_command);
	add_new_command("date", "Outputs system time as date format", date_command);
	add_new_command("clear", "Clear terminal", clear_command);
	add_new_command("asmjit", "Starts JIT prompt", asmjit_command);
	add_new_command("tick", "Prints current tick count from PIT", tick_command);
	add_new_command("snake", "Have some fun!", snake_command);
	add_new_command("shutdown", "Shutdown PC", shutdown_command);
	add_new_command("gfxtest", "Run graphics tests", test_gfx);
	add_new_command("startx", "Start window manager", startx_command);
	add_new_command("", "", empty_command);
}
