#include "shell.h"
#include <lib/iberty/iberty.h>
#include <std/kheap.h>
#include <std/memory.h>
#include <std/printf.h>
#include <gfx/lib/gfx.h>
#include <user/shell/programs/asmjit/asmjit.h>
#include <user/shell/programs/snake/snake.h>
#include <user/shell/programs/rexle/rexle.h>
#include <user/xserv/xserv.h>
#include <kernel/kernel.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/pci/pci_detect.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/vesa/vesa.h>
#include <tests/test.h>
#include <tests/gfx_test.h>
#include <std/klog.h>
#include <user/programs/usage_monitor.h>

size_t CommandNum;
command_table_t CommandTable[MAX_COMMANDS];
fs_node_t* current_dir;

int findCommand(char* command) {
	for (size_t i = 0; i < CommandNum + 1; i++) {
		int ret = strcmp(command, CommandTable[i].name);

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

	if (((string != NULL) && (string[0] == '\0')) || !strlen(string))
		return;

	int argc;
	char *sdup = strdup(string);
	char **argv = buildargv(sdup, &argc);
	kfree(sdup);

	if (argc == 0) {
		freeargv(argv);
		return;
	}

	char* command = argv[0];

	int i = findCommand(command);
	if (i >= 0) {
		void (*command_function)(int, char **) = (void(*)(int, char**))CommandTable[i].function;
		command_function(argc, argv);
	}

	//cleanup
	freeargv(argv);
}

static int visible_input_len = 0;
static void predict_command(char* name_buf, int name_len, char* description_buf, int description_len, char* input) {
	memset(name_buf, 0, name_len);
	memset(description_buf, 0, description_len);

	//distance from first char to first occurance of first char of input
	//this is a heuristic for closest match
	int best_match_distance = 1000;
	//index in command table of best match
	int best_match_idx = -1;

	//look through every command in command table
	//see if any contain input as a substring
	//if so, it's a safe guess
	//TODO this just uses the first result, improve this?
	for (size_t i = 0; i < CommandNum; i++) {
		char* command = CommandTable[i].name;
		char* pos = strstr(command, input);
		if (pos != NULL) {
			//this command contained match of input
			//look at how far match was from start of command name
			int length = pos - command;
			printk("predict_command(): found fuzzy match of %s to %s (distance %d)\n", input, command, length);
			//is this the closest match we've found?
			if (length < best_match_distance) {
				best_match_distance = length;
				best_match_idx = i;
			}
		}
	}

	//did we find a match?
	if (best_match_idx != -1) {
		char* command = CommandTable[best_match_idx].name;
		printk("predict_command(): best match of %s was %s\n", input, command);
		strcpy(name_buf, command);
		strcpy(description_buf, CommandTable[best_match_idx].description);
	}
}

void process_character(char* inputstr, char ch) {
	//handle escapes
	if (ch == '\033') {
		//skip [
		getchar();
		char ch = getchar();
		switch (ch) {
			//up arrow
			case 'A':
				term_scroll(TERM_SCROLL_UP);
				break;
			//down arrow
			case 'B':
				term_scroll(TERM_SCROLL_DOWN);
				break;
			//right arrow
			case 'C':
				break;
			//left arrow
			case 'D':
				break;
		}
	}
	//handle backspace
	else if (ch == '\b') {
		//remove last character from input string
		if (visible_input_len > 0) {
			char lastChar = inputstr[strlen(inputstr)-1];
			//if we're removing a space, check if we should reset the color to indicate a command
			if (lastChar == ' ') {
				//TODO make sure there's only one word left before resetting color
				printf("\e[9;");
			}

			inputstr = delchar(inputstr);
			//terminal driver will remove last char
			putchar(ch);
			visible_input_len--;
		}
	}
	//handle newline
	else if (ch == '\n') {

	}
	//handle tab
	//predict input
	else if (ch == '\t') {
		char prediction[64];
		char description[128];
		predict_command((char*)&prediction, 64, (char*)&description, 128, inputstr);
		//did we get a prediction?
		if (*prediction) {
			//remove existing input and print prediction
			for (size_t i = 0; i < strlen(inputstr); i++) {
				putchar('\b');
			}
			strcpy(inputstr, prediction);
			printf("%s \e[7;(%s)", inputstr, description);
			visible_input_len = strlen(inputstr) + strlen(description) + 3;
		}
	}
	else  {
		//add this character to the input string and output it
		strccat(inputstr, ch);
		putchar(ch);
		visible_input_len++;
	}

	//if this character was a space, change text color to indicate an argument
	if (ch == ' ') {
		printf("\e[13;");
	}
}

char* get_inputstring() {
	const int max_chars = 128;
	char* input = kmalloc(max_chars);
	unsigned char c = 0;

	for (int i = 0; i < max_chars; i++) {
		c = getchar();
		process_character(input, c);
		if (c == '\n') {
			break;
		}
	}

	return input;
}

int shell() {
	//reset terminal color in case it was changed
	//then set to input color
	printf("\e[10;axle> \e[9;");

	char* input = get_inputstring();

	//set terminal color to stdout color
	printf("\e[15;");
	process_command(input);

	if (strcmp(input, "shutdown") == 0) {
		KLOG(kfree, input);
		return 1;
	}
	KLOG(kfree, input);
	printf("\n");
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
		int spaces_needed = 11 - strlen(CommandTable[i].name);
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
	char buf[64];
	memset(&buf, 0, sizeof(buf));
	date((char*)&buf);
	printf(buf);
}

void empty_command() {
	//do nothing if nothing was entered
}

void script_command(int argc, char** argv) {
	if (argc < 2) {
		printf_err("Please specify a script");
		return;
	}
	
	char* file = argv[1];
	fs_node_t* node = finddir_fs(current_dir, file);
	if (!node) {
		printf_err("File %s not found");
		return;
	}
	
	//Read file line-by-line and use each line as a command
	uint8_t filebuf[128][2048];
	memset(&filebuf, 0, sizeof(filebuf) / sizeof(filebuf[0]));
	uint32_t sz = read_fs(node, 0, 2048, (uint8_t*)&filebuf);
	for (uint32_t i = 0; i < sz; i++) {
		process_command((char*)filebuf[i]);
	}
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
	printf_info("Press 'q' to exit xserv");
	printf_info("Press any key to continue");
	getchar();

	//spawn xserv into its own process
	int xserv_pid = fork("xserv");
	if (xserv_pid) {
		return;
	}

	//display boot screen
	display_boot_screen();
	//actually launch xserv
	xserv_init();
}

void rexle_command() {
	printf_info("Press 'q' to exit");
	printf_info("Move with WASD");
	printf_info("Press any key to continue");
	getchar();

	rexle();
}

void ls_command() {
	//list contents of current directory
	int i = 0;
	struct dirent* node = 0;
	while ((node = readdir_fs(current_dir, i)) != 0) {
		fs_node_t* fsnode = finddir_fs(current_dir, node->name);
		if ((fsnode->flags & 0x7) == FS_DIRECTORY) {
			printf("(dir)  %s/\n", node->name);
		}
		else {
			printf("(file) %s\n", node->name);
		}
		i++;
	}
}

void cat_command(int argc, char** argv) {
	if (argc < 2) {
		printf_err("Please specify a file");
		return;
	}
	char* file = argv[1];
	fs_node_t* node = finddir_fs(current_dir, file);
	if (!node) {
		printf_err("File %s not found");
		return;
	}
	uint8_t filebuf[2048];
	memset(filebuf, 0, 2048);
	uint32_t sz = read_fs(node, 0, 2048, filebuf);
	for (uint32_t i = 0; i < sz; i++) {
		putchar(filebuf[i]);
	}
}

void hex_command(int argc, char** argv) {
	if (argc < 2) {
		printf_err("Please specify a file");
		return;
	}
	char* file = argv[1];
	fs_node_t* node = finddir_fs(current_dir, file);
	if (!node) {
		printf_err("File %s not found");
		return;
	}
	uint8_t filebuf[8];
	memset(filebuf, 0, 8);
	uint32_t sz = read_fs(node, 0, 8, filebuf);
	for (uint32_t i = 0; i < sz; i++) {
		printf("%x ", filebuf[i]);
	}
}

void cd_command(int argc, char** argv) {
	if (argc < 2) {
		printf_err("Please specify a directory");
		return;
	}

	char* dest = argv[1];
	fs_node_t* new_dir = finddir_fs(current_dir, dest);
	if (new_dir) {
		current_dir = new_dir;
		return;
	}
	printf_err("Directory %s not found", dest);
}

void pwd_command() {
	array_m* parents = array_m_create(16);

	//find all parent directories up to root
	fs_node_t* parent = current_dir->parent;
	if (parent) {
		do {
			array_m_insert(parents, parent->name);
		} while ((parent = parent->parent));
	}

	//print out all parent directories, starting with the topmost
	for (int i = parents->size - 1; i >= 0; i--) {
		printf("%s/", array_m_lookup(parents, i));
	}
	printf("%s", current_dir->name);
}

void open_command(int argc, char** argv) {
	if (argc < 2) {
		printf_err("Please specify a directory");
		return;
	}

	// loader_init();
	// elf_init();

	char* name = argv[1];
	fs_node_t* file = finddir_fs(current_dir, name);
	if (file) {
		uint8_t* filebuf = (uint8_t*)kmalloc(8192);
		memset(filebuf, 0, 8192);
		//fs_node_t* file = fopen(name, 0);
		// uint32_t sz = read_fs(file, 0, 8192, filebuf); //TODO: make use of it?

		// exec_start(filebuf);
		return;
	}
	printf_err("File %s not found", name);
}

void hypervisor_command() {
	printf("(0x1210) MOV R0, #1\n");
	printf("(0x1020) MOV R1, #2\n");
	printf("(0x2302) ADD R2, R0 + R1\n");
	printf("(0x4303) MUL R2, R1 * R2\n");
	printf("(0x1210) MOV R3, #1\n");
	printf("(0x3332) SUB R2, R2 - R3\n");
	printf("(0x0000) END\n");
	printf("--- cpu state: R0 = 1 R1 = 2 R2 = 5 R3 = 1 ---\n");
}

void proc_command() {
	proc();
	printf_info("Process state logged");
}

void shell_init() {
	printf("\n");
	printf_info("Boostrap complete.");
	printf_info("Type 'help' for a list of available commands");
	printf_info("Try 'startx' to launch axle's window manager.");
	printf_info("axle's shell supports autocomplete on <tab>. Try it!");
	//set shell color
	printf("\e[10;");

	//set up command table
	add_new_command("help", "Display help information", help_command);
	add_new_command("echo", "Outputs args to stdout", (void(*)())echo_command);
	add_new_command("time", "Outputs system time", time_command);
	add_new_command("date", "Outputs system time as date format", date_command);
	add_new_command("clear", "Clear terminal", clear_command);
	add_new_command("tick", "Prints current tick count from PIT", tick_command);
	add_new_command("shutdown", "Shutdown PC", shutdown_command);
	add_new_command("gfxtest", "Run graphics tests", test_gfx);
	add_new_command("startx", "Start window manager", startx_command);
	add_new_command("rexle", "Start 3D renderer (pass VGA for VGA mode)", rexle_command);
	add_new_command("heap", "Run heap test", test_heap);
	add_new_command("ls", "List contents of current directory", ls_command);
	add_new_command("cd", "Switch to another directory", (void(*)())cd_command);
	add_new_command("pwd", "Print working directory", pwd_command);
	add_new_command("cat", "Write file to stdout", (void(*)())cat_command);
	add_new_command("hex", "Write hex dump of file to stdout", (void(*)())hex_command);
	add_new_command("open", "Load file", (void(*)())open_command);
	add_new_command("proc", "List running processes", proc_command);
	add_new_command("pci", "List PCI devices", pci_list);
	add_new_command("hypervisor", "Run VM", hypervisor_command);
	add_new_command("script", "Run a script", (void(*)())script_command);
	add_new_command("", "", empty_command);

	//register ourselves as the first responder
	//this makes us the designated target for keystrokes
	become_first_responder();

	//set current dir to fs root
	current_dir = fs_root;
}
