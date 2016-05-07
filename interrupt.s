%macro ISR_NOERRCODE 1 		; define a macro, taking one parameter
	[GLOBAL isr%1] 			; %1 accesses the first parameteer
	isr%1:
		cli 			; disable interrupts
		push byte 0		; push dummy error code (if ISR0 doesn't push its own error code)
		push byte %1 		; push interrupt number (0)
		jmp isr_common_stub 	; go to common handler
%endmacro

%macro ISR_ERRCODE 1
	[GLOBAL isr%1]
	isr%1:
		cli
		push byte %1
		jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

[EXTERN isr_handler]

; common ISR stub. Saves processor state, sets
; up kernel mode segments, calls C-level fault handler,
; and finally restores stack frame
isr_common_stub:
	pusha		; pushes edi, esi, ebp, esp, ebx, edx, ecx, eax
	
	mov ax, ds	; lower 16 bits of eax = ds
	push eax 	; save data segment descriptor

	mov ax, 0x10	; loads kernel data segment argument
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	call isr_handler

	pop eax		; reload original data segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	popa 		; pop edi, esi, ebp, etc
	add esp, 8 	; cleans up pushed error code and pushed ISR number
	sti
	iret		; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
