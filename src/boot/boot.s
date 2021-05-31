; Multiboot v0.6.96 spec: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Header-graphics-fields 
MBOOT_PAGE_ALIGN	equ 1<<0	; load kernel and modules on page boundary
MBOOT_MEM_MAP		equ 1<<1	; provide kernel. with memory info
MBOOT_VIDEO_MODE	equ 1<<2	; have GRUB set video mode
MBOOT_HEADER_MAGIC	equ 0x1BADB002	; multiboot magic value
MBOOT_HEADER_FLAGS	equ MBOOT_PAGE_ALIGN | MBOOT_MEM_MAP | MBOOT_VIDEO_MODE
MBOOT_CHECKSUM		equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

[bits 32]

[section .multiboot_header] 
dd MBOOT_HEADER_MAGIC	; header value for GRUB
dd MBOOT_HEADER_FLAGS	; grub settings
dd MBOOT_CHECKSUM		; ensure above values are correct
dd 0, 0, 0, 0, 0	; Unused flags for specifying load positions
dd 0	; Graphics mode type (0 = linear graphics, 1 = EGA-standard text mode)
dd 1920	; Graphics px width
dd 1080	; Graphics px height
dd 32	; Graphics bits per pixel

[section .text]
[global _start]
[extern kernel_main]
_start:
	mov esp, _kernel_stack_top
	push esp
	push ebx
	; Set ebp to 0. This is a marker for walking stack frames to know that 
	; we've reached the base stack frame
	xor ebp, ebp

	; execute kernel
	call kernel_main

	; once the kernel is done executing, spin forever
	; do so by turning off interrupts, then halting until CPU gets an interrupt 
	cli
	hlt
	jmp $

[section .bss]

[global _kernel_stack_bottom]
[global _kernel_stack_top]

_kernel_stack_bottom:
align 4096 ; ensure stack is page-aligned
resb 16384 ; reserve 16kb stack for kernel
_kernel_stack_top: