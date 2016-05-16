[bits 32]

[section .text]
[global enableA20]

; TODO replace this
; testing code
; stub for checkA20 until we figure out why
; the real code causes boot to hang
; [extern checkA20]
checkA20:
    mov ax, 1
    ret

;subroutine that tries different methods to enable a20 
enableA20:
    call checkA20 ; check if a20 is enabled already
    cmp ax, 0
    jne already_enabled

    ;call bios_a20 ; try bios method 

    call checkA20 ; check if a20 is now enabled 
    cmp ax, 0
    jne enabled

    call kb_cont_a20 ; try keyboard controller method
    ; TODO implement timer with time-out here 
    ; (kb controller method may work slowly)

    call checkA20 ; check if a20 is now enabled
    cmp ax, 0
    jne enabled

    call fast_a20 ; try fast a20 method 
    ; TODO implement timer with time-out here
    ; (fast method may work slowly)

    call checkA20 ; check if a20 is now enabled
    cmp ax, 0
    jne enabled

    call failure ; nothing worked, give up

failure:
    ; TODO implement some type of reporting here
    mov ax, 1
    ret

enabled:
    mov ax, 0
    ret

already_enabled:
    mov ax, 6
    ret

;subroutine that tries to enable a20 with bios method (not universal)
bios_a20:
    mov ax, 0x2401
    int 0x15
    ret

;subroutine to enable A20 gate with keyboard controller
kb_cont_a20:
    cli ; disable interrupts
 
    call a20wait ; wait for kb to finish
    mov al, 0xAD ; disable keyboard
    out 0x64, al
 
    call a20wait ; wait for kb to finish
    mov al, 0xD0 ; read from input
    out 0x64, al
 
    call a20wait2 ; wait for kb to finish
    in al, 0x60 ; get input
    push eax
 
    call a20wait ; wait for kb to finish
    mov al, 0xD1 ; write to output
    out 0x64, al
 
    call a20wait ; wait for kb to finish
    pop eax ; output (input | 2)
    or al, 2
    out 0x60, al
    
    call a20wait ; wait for kb to finish
    mov al, 0xAE ; enable keyboard
    out 0x64, al
 
    call a20wait ; wait for kb to finish
    sti ; reenable interrupts
    ret
 
a20wait: 
    in al, 0x64
    test al, 2
    jnz a20wait
    ret
 
a20wait2:
    in al, 0x64
    test al, 1
    jz a20wait2
    ret

;subroutine to enable a20 with fast method
fast_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

 
 
 