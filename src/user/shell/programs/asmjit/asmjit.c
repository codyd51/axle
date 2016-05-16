#include "asmjit.h"
#include <std/std.h>

/*
//allocates RW mem of given size and returns pointer to it. 
//on failure, prints out the rror and returns NULL
//unlike malloc, the mem is allocated on
//a page boundary so it's suitable for calling memprotect
void* alloc_writable_memory(size_t size) {
	void* ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == (void*)-1) {
		perror("map");
		return NULL;
	}
	return ptr;
}

//sets RX permission on given memory, which must be page-aligned
//returns 0 on success
//on failure, prints out error and returns -1
int make_memory_executable(void* m, size_t size) {
	if (mprotect(m, size, PROT_READ | PROT_EXEC) == -1) {
		perror("mprotect");
		return -1;
	}
	return 0;
}

const size_t SIZE = 1024;
typedef long (*JittedFunc)(long);

//allocates RW memory, emits code into it, and sets it to 
//RX before executing
void emit_to_rw_run_from_rx() {
	void* m = alloc_writeable_memory(SIZE);
	emit_code_into_memory(m);
	make_memory_executable(m, SIZE);

	JittedFunc func = m;
	int result = func(2);
	printf("result = %d\n", result);
}
*/

void asmjit() {
	char ch = 0;
	//EOF = -1
	while ((ch = getchar()) != 'q') {
		printf("%c\n", ch);
	}
	printf("got eof\n");
}

