#include "axle.h"

#define SPIN while (1) {sys_yield(RUNNABLE);}
#define SPIN_NOMULTI while (1) {}

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

bool run_shmem_test() {
	int mem_size = 0x100;
	page_directory_t* dir = page_dir_current();
	printf_info("shmem testing mem_size ");
	for (int i = 0; i < 2; i++) {
		printf("%d-", mem_size);
		char* kernel_mem_addr = NULL;
		char* new_mem = shmem_get_region_and_map(dir, mem_size, 0x0, &kernel_mem_addr, true);
		if (!new_mem || !kernel_mem_addr) {
			return false;
		}

		memset(new_mem, 'x', mem_size);
		//kfree(kernel_mem_addr);

		mem_size *= 2;
	}
	printf("\n");
	return true;
}

bool run_module_tests() {
	if (!run_shmem_test()) {
		printf_err("shmem test failed!\n");
		return false;
	}
	else {
		printf_dbg("shmem test passed");
	}
	return true;
}
/*
 * Multiboot spec module structure:
             +-------------------+
     0       | mod_start         |
     4       | mod_end           |
             +-------------------+
     8       | string            |
             +-------------------+
     12      | reserved (0)      |
             +-------------------+
 */
typedef struct multiboot_module {
	//physical address of module head
	uint32_t	mod_start;
	//physical address of module tail
	//
	uint32_t	mod_end;
	//unique module identifier
	char*		cmdline;
	uint32_t	mmo_reserved;
} multiboot_module_t;

static multiboot_module_t boot_modules[16] = {0};
static int boot_modules_count = 0;

void kernel_bootmod_init(multiboot* mboot_ptr) {
	memset(boot_modules, 0, sizeof(boot_modules));

	multiboot_module_t* mod = (multiboot_module_t*)mboot_ptr->mods_addr;
	for (int i = 0; i < (int)mboot_ptr->mods_count; i++) {
		boot_modules[i].mod_start = mod->mod_start;
		boot_modules[i].mod_end = mod->mod_end;
		boot_modules[i].cmdline = mod->cmdline;

		mod++;
	}
	boot_modules_count = mboot_ptr->mods_count;
}

bool boot_stage1(multiboot* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;

	kernel_process_multiboot(mboot_ptr);
	kernel_bootmod_init(mboot_ptr);

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
	printf_info("%d boot modules detected", boot_modules_count);
	printf ("mods_count = %d, mods_addr = 0x%x\n",
			(int) mboot_ptr->mods_count, (int) mboot_ptr->mods_addr);
	for (int i = 0; i < boot_modules_count; i++) {
		multiboot_module_t mod = boot_modules[i];
		printf("mod %d: mod_start = %x, mod_end = %x, cmdline = %s\n", i,
			   (unsigned)mod.mod_start,
			   (unsigned)mod.mod_end,
			   (char *)mod.cmdline);
	}
	paging_install();

	printf_info("boot phase 1 ok, rtc,pit,paging set up");
	return true;
}

bool boot_stage2(void) {
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

	printf_info("boot phase 2 ok, processes,syscalls,initrd,drivers set up");
	return true;
}

bool boot_stage3(void) {
	bool tests_succeeded = run_module_tests();
	ASSERT(tests_succeeded, "At least one kernel module test failed, halting");

	/*
	void sys_execve(char*, void*, int);
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

	printf_info("boot stage 3 returned?");
	return true;
}

void kernel_main(multiboot* mboot_ptr, uint32_t initial_stack) {
	if (!boot_stage1(mboot_ptr, initial_stack)) {
		ASSERT(0, "boot stage 1 failed");
	}
	if (!boot_stage2()) {
		ASSERT(0, "boot stage 2 failed");
	}
	SPIN_NOMULTI;
	if (!boot_stage3()) {
		ASSERT(0, "boot stage 3 failed");
	}

	//wait for all children processes to finish
	wait(NULL);

	//all tasks have died, kill kernel
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
