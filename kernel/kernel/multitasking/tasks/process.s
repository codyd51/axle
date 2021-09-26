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
	;mov eax, [esp]
	;ret
	pop eax
	jmp eax

[GLOBAL task_switch_real]
task_switch_real:
	;xchg bx, bx
	cli
	mov ecx, [esp + 4]	; eip
	mov eax, [esp + 8]	; physical address of current paging dir
	mov ebp, [esp + 12] ; ebp
	mov esp, [esp + 16] ; esp
	mov cr3, eax		; set paging directory
	mov eax, 0xDEADBEEF	; magic value to detect task switch
	sti
	jmp ecx
