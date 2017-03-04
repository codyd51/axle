[SECTION .text]
[GLOBAL start]			; entry point
[GLOBAL _start]			; entry point
[EXTERN main]
start:
_start:
	call main
	ret
