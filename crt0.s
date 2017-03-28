[SECTION .text]
[GLOBAL start]			; entry point
[GLOBAL _start]			; entry point
[EXTERN main]
start:
_start:
	;set up stack frame
	push ebp
	mov ebp, esp

	;push argc and argv
	push dword [ebp+12]
	push dword [ebp+8];

	;jump to main
	call main
	; put return code in ebx
	mov ebx, eax

	; exit is syscall # 12
	mov eax, 12
	int 0x80
