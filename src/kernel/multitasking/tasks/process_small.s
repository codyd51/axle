[EXTERN _current_task_small]
[EXTERN task_small_offset_to_context]
[GLOBAL context_switch]
context_switch:
    mov ecx, [esp + 0x4] ; save new task argument
;Save the old task's state
   ;Save registers that are supposed to be callee-saved
   push eax
   push ebx
   push esi
   push edi
   push ebp

; 0x6c is the offset to _current_task_small->context! FIX ME PT
   mov esi,[_current_task_small]  ;Get address of old task's "thread control block" from global variable
   mov [esi+0x6c],esp   ;Save old task' ESP in its "thread control block"

;Load the new task's state

   mov edi,ecx         ;Get address of new task's "thread control block" from arg
   mov [_current_task_small], edi  ;Set address of new task's "thread control block" in global variable for later
   mov esp,[edi+0x6c]   ;Load new ESP from new task's "thread control block"

   ;Restore registers that were callee-saved

   pop ebp
   pop edi
   pop esi
   pop ebx
   pop eax

   ;Return to the new task's EIP (that was stored on its stack

   sti
   ret