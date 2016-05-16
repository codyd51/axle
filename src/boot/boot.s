#Declare constants used for creating multiboot header
.set ALIGN,     1<<0                #align loaded modules on page boundaries
.set MEMINFO,   1<<1                #provide memory map
.set FLAGS,     ALIGN | MEMINFO     #multiboot 'flag' field
.set MAGIC,     0x1BADB002          #lets bootloader find header
.set CHECKSUM,  -(MAGIC+FLAGS)      # prove we're multiboot

#declare header as in multiboot standard
#bootloader will search for this magic sequence and recognize a multiboot kernel
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

#currently the stack pointer reg (esp) points to anything and shoudl not be used
#so, let's provide our own stack
#allocate room for a small temporary stack by creating a symbol at the bottom of it,
#allocating 16384 bytes for it, and creating a symbol at the top
.section .bootstrap_stack, "aw", @nobits
stack_bottom:
.skip 16384 #16kb
stack_top:

#linker script specifies _start as the entry point 
#the bootloader will jump to this position once the kernel has been loaded.
#it doesn't make sense to return from this function as the bootloader is gone
.section .text
.global _start
.type _start, @function

_start:

    #let's set the esp register to
    #point to the top of our stack (grows downwards)
    movl $stack_top, %esp

    #push multiboot header location
    push %ebx

    #we can now actually execute C code!
    call kernel_main

    #in case that function returns, we want an infinite loop
    #the halt instruction to stop the CPU until the next interrupt arrives,
    #jumping to the halt instruction if it ever continues execution
    cli
    hlt
.Lhang:
    jmp .Lhang

#set size of the _start symbol to the current location '.' minus its start
.size _start, . -_start






