[GLOBAL idt_flush] 	; allow this to be called from C
idt_flush:
	mov eax, [esp+4] 	; get the pointer to the IDT, passed as a parameter
	lidt [eax] 		; load IDT pointer
	ret

[GLOBAL tss_flush]
tss_flush:
	mov ax, 0x2B		; load index of TSS structure
				; index is 0x28 (5th selector, each 8 bytes)
				; but set the bottom two bits to set RPL 
				; to 3, not 0
	ltr ax			; load 0x2B into task state register
	ret
