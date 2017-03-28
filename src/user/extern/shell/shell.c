#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#include "lib/iberty/iberty.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void prompt() {
	printf("me@axle[%d] /$ ", getpid());
	fflush(stdout);
}

int get_inputstring(char* buf, int len) {
	memset(buf, 0, len);

	int count = read(0, buf, len);
	buf[count] = '\0';
	return count;
}

typedef int (*command_func)(int, char**);
typedef struct command {
	char* name;
	char* description;
	command_func func;
} command_t;
#define MAX_COMMANDS 64
static command_t cmdtable[MAX_COMMANDS];
static int cmdcount = 0;

void register_command(char* name, char* description, command_func func) {
	cmdtable[cmdcount].name = name;
	cmdtable[cmdcount].description = description;
	cmdtable[cmdcount].func = func;
	cmdcount++;
}

int find_command(char* command) {
	for (size_t i = 0; i < cmdcount; i++) {
		int ret = strcmp(command, cmdtable[i].name);
		if (ret == 0) {
			return i;
		}
	}
	return -1;
}

//holds exit code of last run command
static int exit_code = 0;
int process_command(char* input) {
	if (((input != NULL) && (input[0] == '\0')) || !strlen(input))
		return;

	int argc;
	char *sdup = strdup(input);
	char **argv = buildargv(sdup, &argc);
	free(sdup);

	int index = find_command(argv[0]);

	if (index == -1) {
		//TODO check if valid file!
		int pid = fork();
		if (!pid) {
			FILE* attempt = fopen(input, "r");
			if (attempt) {
				fclose(attempt);
				execve(argv[0], argv, 0);
				_exit(1);
			}
			else {
				printf("Command %s not found.\n", input);
				return -1;
			}
		}
		else {
			int status;
			pid = waitpid(pid, &status, 0);
			freeargv(argv);
			return status;
		}
		//printf("Command %s not found.\n", input);
		//return -1;
	}

	command_func func = cmdtable[index].func;
	int ret = func(argc, argv);
	freeargv(argv);

	return ret;
}

int help(int argc, char** argv) {
	printf("All commands listed here are internally defined.\n");
	printf("Type 'help' to see this list\n");
	for (size_t i = 0; i < cmdcount; i++) {
		int spaces_needed = 11 - strlen(cmdtable[i].name);
		printf("\n\t%s", cmdtable[i].name);
		for (int i = 0; i < spaces_needed; i++) {
			putchar(' ');
		}
		printf("%s", cmdtable[i].description);
	}
	putchar('\n');
	return 0;
}

static bool running = true;
int quit(int argc, char** argv) {
	printf("Quitting shell...\n");
	running = false;
	return 0;
}

int empty(int argc, char** argv) {
	return exit_code;
}

int query_exit(int argc, char** argv) {
	return exit_code;
}

int main(int argc, char** argv) {
	memset(cmdtable, 0, sizeof(command_t) * MAX_COMMANDS);
	register_command("help", "print help", &help);
	register_command("exit", "quit shell", &quit);
	register_command("?", "print exit code of last command", &query_exit);
	register_command("", "", &empty);

	while (running) {
		prompt();

		char input[256];
		get_inputstring((char*)&input, sizeof(input));

		exit_code = process_command(input);
		printf("[%d]\n", exit_code);
	}
	return 0;
}

