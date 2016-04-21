;	Kernel's entry point. We can call main here, or we can setup stack and such
;	maybe set up GDT and segments
;	Interrupts are disabled at this point

[BITS 32]
global start
start:
	mov esp, _sys_stack ;	This points the stack to our new stack area
	jmp stublet

;	This MUST be 4-byte aligned
ALIGN 4
mboot:
	;	Multiboot macros to make a few lines later more readable
	MULTIBOOT_PAGE_ALIGN	equ 1<<0
	MULTIBOOT_MEMORY_INFO	equ 1<<1
	MULTIBOOT_AOUT_KLUDGE	equ 1<<16
	MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
	MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE
	MULTIBOOT_CHECKSUM	equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
	EXTERN code, bss, end

	;	GRUB multiboot header
	;	boot signature
	dd MULTIBOOT_HEADER_MAGIC
	dd MULTIBOOT_HEADER_FLAGS
	dd MULTIBOOT_CHECKSUM

	;	AOUT kludge - must be physical addresses.
	;	Linker script fills in the data for these ones
	dd mboot
	dd code
	dd bss
	dd end
	dd start

;	endless loop
;	later on, insert 'extern_main', followed by 'call_main'
;	right before 'jmp $'
stublet:
	jmp $

;	BSS section
;	for now, just store the stack
;	since stack grows downwards, we declare the size of the data before declaring
;	the identifier '_sys_stack'
SECTION .bss
	;	reserve 8kb of memory for stack
	resb 8192
_sys_stack: