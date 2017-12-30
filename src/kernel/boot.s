# Declare constants for Multiboot header
.set ALIGN,		1 << 0 # align modules on page boundaries
.set MEMINFO,	1 << 1 # give us memory map
.set FLAGS,		ALIGN | MEMINFO 
.set MAGIC,		0x1BADB002 # multiboot magic
.set CHECKSUM, -(MAGIC + FLAGS) # CRC

# put multiboot header in its own section so we can force its location in the linker script
.section multiboot_header
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# set up the stack pointer
# drop a marker, skip some kb, and drop another marker
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16kb
stack_top:

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
	mov $stack_top, %esp

	# do crucial environment setup here!
	# TODO(PT): load GDT here
	# TODO(PT): setup paging here

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
