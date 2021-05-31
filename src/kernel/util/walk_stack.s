; Walks backwards through the call stack and builds a list of return addresses.
; Args:
;  * Array of 32-bit addresses.
;  * Maximum number of elements in array.
; Return value: The number of addresses stored in the array.
; Calling convention: cdecl
; Source: https://wiki.osdev.org/Stack_Trace
[global walk_stack]
walk_stack:
    ; Create stack frame & save caller's EDI and EBX.
    push ebp
    mov  ebp,       esp
    sub  esp,       8
    mov  [ebp - 4], edi
    mov  [ebp - 8], ebx
    ; Set up local registers.
    xor  eax,       eax         ; EAX = return value (number of stack frames found).
    mov  ebx,       [esp +  8]  ; EBX = old EBP.
    mov  edi,       [esp + 16]  ; Destination array pointer in EDI.
    mov  ecx,       [esp + 20]  ; Maximum array size in ECX.
.walk:
    ; Walk backwards through EBP linked list, storing return addresses in EDI array.
    ; Did we reach the "root" base pointer?
    ; (The root base pointer is set up by the kernel bootstrap, and is
    ;  valid only for kernel stack frames, not user-mode stack frames)
    test ebx,       ebx
    jz   .done

    ; Did we hit the maximum stack frame length?
    test ecx,       ecx
    jz .done
    ; Record that we walked one more frame
    dec ecx

    mov  edx,       [ebx +  4]  ; EDX = previous stack frame's IP.
    mov  ebx,       [ebx +  0]  ; EBX = previous stack frame's BP.
    mov  [edi],     edx         ; Copy IP.
    add  edi,       4
    inc  eax
    loop .walk
.done:
    ; Restore caller's EDI and EBX, leave stack frame & return EAX.
    mov  edi,       [ebp - 4]
    mov  ebx,       [ebp - 8]
    leave
    ret