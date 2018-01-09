; export symbol to C code
[global idt_activate]
idt_activate:
    ; pointer to IDT is passed as first parameter on stack
    mov eax, [esp+4]
    ; load dereferenced IDT pointer
    lidt [eax]
    ret