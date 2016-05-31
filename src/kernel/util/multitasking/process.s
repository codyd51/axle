[GLOBAL copy_page_physical]
copy_page_physical:
	push ebx		; preserve contents of EBX
	pushf			; push EFLAGS so we can pop and reenable interrupts
				; if they were enabled
	cli
				; load these before enabling paging
	mov ebx, [esp + 12]	; source address
	mov ecx, [esp + 16]	; destination address

	mov edx, cr0		; control register
	and edx, 0x7fffffff
	mov cr0, edx		; disable paging

	mov edx, 1024		; 1024 * 4 bytes = 4096 (1 page)

.loop:
	mov eax, [ebx]		; get word at source addr
	mov [ecx], eax		; store it at dest addr
	add ebx, 4		; source addr += sizeof(word)
	add ecx, 4		; dest addr += sizeof(word)
	dec edx
	jnz .loop

	mov edx, cr0		; get control register again
	or edx, 0x80000000
	mov cr0, edx		; enable paging

	popf			; pop EFLAGS back
	pop ebx			; restore ebx
	ret

[GLOBAL read_eip]
read_eip:
	pop eax
	jmp eax

[GLOBAL perform_task_switch]
perform_task_switch:
	cli;
	mov ecx, [esp+4]
	mov eax, [esp+8]
	mov ebp, [esp+12]
	mov esp, [esp+16]
	mov cr3, eax
	mov eax, 0xDEADBEEF
	sti
	jmp ecx

