[SECTION .text]
[GLOBAL start]			; entry point
[GLOBAL _start]			; entry point
[EXTERN main]
start:
_start:
	call main
	; put return code in ebx
	mov ebx, eax
	; exit is syscall # 12
	mov eax, 12
	int 0x80
