[GLOBAL tss_flush]
tss_flush:
	mov ax, 0x2B		; load index of TSS structure
				; index is 0x28 (5th selector, each 8 bytes)
				; but set the bottom two bits to set RPL 
				; to 3, not 0
	ltr ax			; load 0x2B into task state register
	ret
