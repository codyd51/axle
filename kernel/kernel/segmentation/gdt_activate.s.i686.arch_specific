; export this symbol to C code
[global gdt_activate]
gdt_activate:
    ; GDT descriptor pointer is passed in eax
    ; read it from stack
    mov eax, [esp+4]
    ; tell the CPU to load it!
    lgdt [eax]

    ; gdt->0x10 is where the kernel data segment is located
    ; load it into all the data segment selectors
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; set CS by jumping to some address where we specify the segment
    ; the offset of the CS segment within the GDT is 0x08
    ; far jump to address in CS!
    jmp 0x08:.flush
.flush:
    ret

[GLOBAL tss_activate]
tss_activate:
	mov ax, 0x2B		; load index of TSS structure
        				; index is 0x28 (5th selector, each 8 bytes)
                        ; but set the bottom two bits to set RPL 
                        ; to 3, not 0
	ltr ax			    ; load 0x2B into task state register
	ret