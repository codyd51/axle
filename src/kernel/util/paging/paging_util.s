[GLOBAL read_cr0]
read_cr0:
	mov eax, cr0
	retn

[GLOBAL write_cr0]
write_cr0:
	push ebp
	mov ebp, esp
	mov eax, [ebp+8]
	mov cr0, eax
	pop ebp
	retn

[GLOBAL read_cr3]
read_cr3:
	mov eax, cr3
	retn

[GLOBAL write_cr3]
write_cr3:
	push ebp
	mov ebp, esp
	mov eax, [ebp+8]
	mov cr3, eax
	pop ebp
	retn

[GLOBAL flush_cache]
flush_cache:
	mov eax, cr3
	mov cr3, eax
	retn
