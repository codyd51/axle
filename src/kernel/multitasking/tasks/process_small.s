[EXTERN _current_task_small]
[EXTERN _task_context_offset]

[GLOBAL context_switch]
context_switch:
    ; Save arg to function containing task to switch to in ecx
    mov ecx, [esp + 0x4]

;Save the old task's state
    ; Save registers that are supposed to be callee-saved
    push eax
    push ebx
    push esi
    push edi
    push ebp

    ; Save preempted task's esp as _current_task->machine_state
    mov eax, [_current_task_small]
    mov ebx, [_task_context_offset]
    add eax, ebx
    mov [eax], esp

;Load the new task's state
    ; Set the provided argument to _current_task_small
    mov [_current_task_small], ecx

    ; Load esp with _current_task->machine_state
    mov ebx, [_task_context_offset]
    add ecx, ebx
    mov esp,[ecx]

    ; Restore registers that were callee-saved
    pop ebp
    pop edi
    pop esi
    pop ebx
    pop eax

    ; Return to the new task's EIP (that was stored on its stack)
    sti
    ret