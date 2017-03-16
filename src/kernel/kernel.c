#include <kernel/kernel.h>
#include "multiboot.h"
#include <stdarg.h>
#include <std/common.h>
#include <std/kheap.h>
#include <std/printf.h>
#include <user/shell/shell.h>
#include <user/xserv/xserv.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include <gfx/font/font.h>
#include <kernel/util/syscall/syscall.h>
#include <kernel/util/syscall/sysfuncs.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/multitasking/tasks/record.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/util/vfs/initrd.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/vesa/vesa.h>
#include <kernel/drivers/pci/pci_detect.h>
#include <kernel/drivers/serial/serial.h>
#include <std/klog.h>
#include <tests/test.h>
#include <kernel/util/ffs/fsdec.h>
#include <kernel/util/ffs/rd_file.h>
#include <kernel/util/multitasking/pipe.h>
#include <kernel/util/unistd/unistd.h>

void print_os_name(void) {
	printf("\e[10;[\e[11;AXLE OS v\e[12;0.6.0\e[10;]\n");
}

void shell_loop(void) {
	int exit_status = 0;
	while (!exit_status) {
		exit_status = shell();
	}

	//give them a chance to recover
	for (int i = 5; i > 0; i--) {
		terminal_clear();
		printf_info("Shutting down in %d. Press any key to cancel", i);
		sleep(1000);
		if (haskey()) {
			//clear buffer
			while (haskey()) {
				getchar();
			}
			//restart shell loop
			terminal_clear();
			shell_loop();
			break;
		}
	}

	//we're dead
	terminal_clear();
	asm("cli");
	asm("hlt");
}

extern uint32_t placement_address;
uint32_t initial_esp;

void module_detect(multiboot* mboot_ptr, uint32_t* initrd_loc, uint32_t* initrd_end) {
	ASSERT(mboot_ptr->mods_count > 0, "no GRUB modules detected");
	*initrd_loc = *((uint32_t*)mboot_ptr->mods_addr);
	*initrd_end = *(uint32_t*)(mboot_ptr->mods_addr+4);
	//don't trample modules
	placement_address = MAX(placement_address, *initrd_end);
}

void kernel_main(multiboot* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;

	//initialize terminal interface
	terminal_initialize();

	//find any loaded grub modules
	//this MUST be done before gfx_init or paging_install
	//otherwise, create_layer has a high chance of overwriting modules
	//module_detect has the side effect of safely incrementing placement_address past any module data
	uint32_t initrd_loc, initrd_end;
	module_detect(mboot_ptr, &initrd_loc, &initrd_end);
	
	//set up graphical terminal
	gfx_init(mboot_ptr);

	//introductory message
	print_os_name();
	test_colors();

	printf_info("Available memory:");
	printf("%d -> %dMB\n", mboot_ptr->mem_upper, (mboot_ptr->mem_upper/1024));

	//descriptor tables
	gdt_install();
	idt_install();

	//serial output for syslog
	serial_init();

	//timer driver (many functions depend on timer interrupt so start early)
	pit_install(1000);
	//wall clock driver
	rtc_install();

	//utilities
	paging_install();
	sys_install();

	//choose scheduler policy here!
	//MLFQ policy: PRIORITIZE_INTERACTIVE
	//round-robin policy: LOW_LATENCY
	tasking_install(LOW_LATENCY);

	//drivers
	kb_install();
	mouse_install();
	pci_install();

	//init ramdisk filesystem,
	//and set up filesystem root
	fs_root = initrd_install(initrd_loc);
	//fs_file* root = NULL;
	//fsDeserialize(initrd_loc, initrd_end - initrd_loc, &root);

	//test facilities
	test_heap();
	test_printf();
	test_time_unique();
	test_malloc();
	test_crypto();

	/*
	//kernel shell
	if (!fork("kern shell")) {
		//start shell
		shell_init();
		shell_loop();
	}

	//test task to simply sleep, print, and quit
	//useful for ensuring multitasking/PIT driver/task blocking
	//work as expected
	if (!fork("sleepy")) {
		sleep(20000);
		printf_dbg("Sleepy thread slept!");
		sys__exit(0);
	}

	//launch ELF shell
	//this is non-kernel code, loaded from filesystem
	if (!sys_fork()) {
		execve("shell", 0, 0);
		sys__exit(1);
	}
	*/

	/*
	int pipes[2];
	pipe(&pipes);

	int pid = sys_fork();
	if (!pid) {
		become_first_responder();
		//sleep(2000);
		//close unused write end
		close(pipes[1]);
		
		//close(0);
		//int d = dup(pipes[0]);
		//printf("dup(%d) = %d\n", pipes[0], d);

		char buf;
		//while (read(pipes[0], &buf, 1) > 0) {
		while (read(pipes[0], &buf, 1)) {
			printf("%c", buf);
		}
		printf("\n");

		close(pipes[0]);
		sys__exit(0);
	}
	else {
		//close unused read end
		close(pipes[0])
		char* message = "This message was sent over a pipe";
		write(pipes[1], message, strlen(message));
		close(pipes[1]);
	}
	*/
	//char* msg = "written with write\nline2\n";
	//write(stdout, msg, strlen(msg));

	become_first_responder();

	int pipefd[2];
	pipe(&pipefd);
	if (!sys_fork()) {
		sleep(500);
		//close unused pipe write
		close(pipefd[1]);
		//replace stdin with pipe read
		dup2(pipefd[0], 0);

		/*	
		char ch;
		while (read(0, &ch, 1) > 0) {
			printf("read %c\n", ch);
		}
		*/
		//become_first_responder();
		char ch;
		while ((ch = getchar()) != EOF) {
			printf("read %c\n", ch);
		}
		close(pipefd[0]);
		sys__exit(0);
	}
	//close unused pipe read
	close(pipefd[0]);
	write(pipefd[1], "hello!\n", strlen("hello!\n"));
	close(pipefd[1]);
	
	/*
	//write(0, "test", strlen("test"));
	while (1) {
		char ch = getchar();
		printf("got %c\n", ch);
	}
	*/

	wait(NULL);

	//done bootstrapping, kill process
	_kill();

	//this should never be reached as the above call never returns
	//if it does, assert
	ASSERT(0, "Kernel bootstrap ran past _kill, is tasking broken?");

	//if by some act of god we've reached this point, 
	//just clear all interrupts and wait forever
	//turn off interrupts
	asm volatile("cli");
	//sleep CPU until next interrupt (never)
	asm volatile("hlt");
	while (1) {}
}

