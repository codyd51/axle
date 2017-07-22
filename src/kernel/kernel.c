#include "axle.h"

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

static elf_t kern_elf_sym;
elf_t* kern_elf() {
	return &kern_elf_sym;
}

extern uint32_t placement_address;
uint32_t initial_esp;

void module_detect(multiboot* mboot_ptr, uint32_t* initrd_loc, uint32_t* initrd_end) {
	ASSERT(mboot_ptr->mods_count > 0, "no GRUB modules detected");
	*initrd_loc = *((uint32_t*)mboot_ptr->mods_addr);
	*initrd_end = *(uint32_t*)(mboot_ptr->mods_addr+4);
	//don't trample modules
	placement_address = *initrd_end;
}

void _int_heap_stability_test() {
	for (int i = 2; i < 4096*16*16*16; i*=1.5) {
		char* mem = kmalloc(i);
		memset(mem, 0, i);
		printf("malloc: %x\n", mem);
		kfree(mem);
	}
	printf("success\n");
	while (1) {}
}

static multiboot* kernel_mboot_ptr = NULL;
static void set_kernel_mboot_ptr(multiboot* m) {
	kernel_mboot_ptr = m;
}

multiboot* kernel_multiboot_ptr() {
	return kernel_mboot_ptr;
}

uint32_t system_mem() {
	return kernel_multiboot_ptr()->mem_upper;
}

static uint32_t initrd_start, initrd_end;
void initrd_loc(uint32_t* start, uint32_t* end) {
	*start = initrd_start;
	*end = initrd_end;
}

void kernel_process_multiboot(multiboot* mboot_ptr) {
	set_kernel_mboot_ptr(mboot_ptr);

	//find any loaded grub modules
	//this MUST be done before gfx_init or paging_install
	//otherwise, create_layer has a high chance of overwriting modules
	//module_detect has the side effect of safely incrementing placement_address past any module data
	module_detect(mboot_ptr, &initrd_start, &initrd_end);

	//initialize terminal interface
	terminal_initialize();

	//set up graphical terminal
	gfx_init(mboot_ptr);

	//parse kernel ELF symbols for debugging purposes
	elf_from_multiboot(mboot_ptr, &kern_elf_sym);

	//mem_upper / 1024 converts kb to mb
	printf_info("%f MB of memory available", mboot_ptr->mem_upper / 1024.0);
	
}

void kernel_main(multiboot* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;

	kernel_process_multiboot(mboot_ptr);

	//introductory message
	print_os_name();
	test_colors();

	//descriptor tables
	descriptor_tables_install();

	//serial output for syslog
	serial_init();

	//timer driver (many functions depend on timer interrupt so start early)
	pit_install(1000);
	//wall clock driver
	rtc_install();

	//utilities
	paging_install();

	//map ramdisk to 0xE0001000
	//heap max addr is at 0xDFFFF000, so this is placed just after that
	//relocated stack is 0xE0000000
	//stack grows downwards, so anything above 0xE0000000 should be unused
	//just to be safe, skip a page 
	uint32_t initrd_start, initrd_end;
	initrd_loc(&initrd_start, &initrd_end);
	initrd_install(initrd_start, initrd_end, 0xE0001000);

	sys_install();
	//choose scheduler policy here!
	//MLFQ policy: PRIORITIZE_INTERACTIVE
	//round-robin policy: LOW_LATENCY
	tasking_install(LOW_LATENCY);

	//drivers
	kb_install();
	mouse_install();
	pci_install();
	ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);

	/*
	//test facilities
	test_heap();
	test_printf();
	test_time_unique();
	test_malloc();
	test_crypto();
	*/

	//fat_install(0, true);

	void sys_execve(char*, void*, int);
	//xserv_init();

	int pid = sys_fork();
	char* argv[] = {"ash", NULL};
	if (!pid) {
		sys_execve(argv[0], argv, 0);
	}
	int stat;
	waitpid(pid, &stat, 0);
	printf("%s returned %d\n", argv[0], stat);

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

void rect_printf(Rect r) {
	printf("{{%d,%d},{%d,%d}}\n", r.origin.x,
								  r.origin.y,
								  r.size.width,
								  r.size.height);
}
