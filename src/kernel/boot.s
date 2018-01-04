# Declare constants for Multiboot header
.set ALIGN,		1 << 0 # align modules on page boundaries
.set MEM_INFO,	1 << 1 # give us memory map
.set VID_INFO,   1 << 2 # set video mode and give us video mode info
.set FLAGS,		ALIGN | MEM_INFO #| VID_INFO
.set MAGIC,		0x1BADB002 # multiboot magic
.set CHECKSUM, -(MAGIC + FLAGS) # CRC

# put multiboot header in its own section so we can force its location in the linker script
.section .multiboot_header
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# set up the stack pointer
# drop a marker, skip some kb, and drop another marker
.section .bss
.align 4096

.global kernel_stack
.global kernel_stack_bottom

kernel_stack_bottom:
.skip 16384 # 16kb
kernel_stack:

# this is the entry point we define in the linker script!
.section .text
.extern kernel_main
.global _start
.type _start, @function
_start:
	# 32 bit protected mode
	# interrupts are off
	# paging is off

	# set esp to the stack we defined in .bss
	mov $kernel_stack, %esp

	# do crucial environment setup here!
	# TODO(PT): load GDT here
	# TODO(PT): setup paging here

	# push multiboot struct to stack
	# this is so it's passed as an argument to kernel_main
	push %ebx

	# C entry point
	call kernel_main

	# kernel has returned!
	# spinloop forever
	cli
1:
	hlt
	jmp 1b

# set size of _start symbol
.size _start, . - _start
