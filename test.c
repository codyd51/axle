#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test = 5;
int main(int argc, char** argv) {
	/*
	printf("Enter a number:");
	//fflush(stdout);
	int d = 0;
	d = 5;
	//scanf("%d", &d);
	char buf[128] = {0};
	//int count = read(0, &buf, sizeof(buf));
	scanf("%s", (char*)&buf);
	//buf[count] = '\0';
	//d = atoi(&buf);
	//scanf("%s", (char*)&buf);
	for (int i = 0; i < d; i++) {
		printf("%d\n", i);
	}
	*/

	printf("Enter your name: ");
	fflush(stdout);
	char buf[128] = {0};
	scanf("%127s", (char*)buf);
	printf("strlen(buf) = %d\n", strlen(buf));
	for (int i = 0; i < strlen(buf); i++) {
		printf("%c", buf[i]);
	}
	char* test = "dank meme";
	for (int i = 0; i < strlen(test); i++) {
		buf[i] = test[i];
	}
	buf[strlen(test)] = '\0';
	printf("Nice to meet you, %s!\n", buf);
	return 0;
}

