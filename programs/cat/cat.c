#include <stdio.h>
#include <unistd.h>

int cat(char* name) {
	FILE* f = fopen(name, "r");
	if (!f) {
		//file not found!
		printf("cat: %s: No such file or directory\n", name);
		return 1;
	}
	char line[256];
	while (fgets(line, sizeof(line), f)) {
			printf("%s", line);
	}
	fclose(f);
	return 0;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("Usage: cat [file]\n");
		return 0;
	}
	
	int succeeded = 0;
	for (int i = 1; i < argc; i++) {
		char* input = argv[i];

		int status = cat(input);
		//only save failure (return code 1)
		//default is success
		if (!succeeded) succeeded = status;
	}
	return succeeded;
}

