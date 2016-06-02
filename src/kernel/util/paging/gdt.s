[GLOBAL gdt_flush] 	; allow this to be called from C
gdt_flush:
	mov eax, [esp+4]	; get pointer to GDT, passed as parameter
	lgdt [eax]		; load new GDT pointer

	mov ax, 0x10		; 0x10 is offset in the GDT to data segment
	mov ds, ax		; load all data segment selectors
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	jmp 0x08:.flush		; 0x08 is the offset to our code segment: far jump!
.flush:
	ret

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
