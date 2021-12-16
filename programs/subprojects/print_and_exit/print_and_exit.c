#include <stdio.h>

int main(int argc, char** argv) {
	printf("**** Running! ****\n");
	printf("PID %d a\nnew\n", getpid());
	printf("Argc: %d argv: 0x%08x\n", argc, argv);
	for (int i = 0; i < argc; i++) {
		printf("Argv[%d] = %s\n", i, argv[i]);
	}
	return 1;
}

