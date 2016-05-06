#Declare constants used for creating multiboot header
.set ALIGN,     1<<0                #align loaded modules on page boundaries
.set MEMINFO,   1<<1                #provide memory map
.set FLAGS,     ALIGN | MEMINFO     #multiboot 'flag' field
.set MAGIC,     0x1BADB002          #'magic number' lets bootloader find header
.set CHECKSUM,  -(MAGIC+FLAGS)      # checksum of above, to prove we're multiboot

#declare header as in multiboot standard. Put this into a special section
#so we can force the header to be in the start of the final program.
#again, these are just magic values from multiboot standard, nothing special going on
#bootloader will search for this magic sequence and recognize a multiboot kernel
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

#currently the stack pointer reg (esp) points to anything and shoudl not be used
#so, let's provide our own stack
#we'll allocate room for a small temporary stack by creating a symbol at the bottom of it,
#then allocating 16384 bytes for it, and finally creating a symbol at the top
.section .bootstrap_stack, "aw", @nobits
stack_bottom:
.skip 16384 #16kb
stack_top:

#linker script specifies _start as the entry point to the kernal, and
#the bootloader will jump to this position once the kernel has been loaded.
#it doesn't make sense to return from this function as the bootloader is gone
.section .text
.global _start
.type _start, @function
_start:
    #welcome to kernel mode! We now have sufficient code for the bootloader to
    #load and run our operating system. It doesn't do much yet, though.

    #to set up a stack so that we can at least use C, let's set the esp register to
    #point to the top of our stack (grows downwards)
    movl $stack_top, %esp

    #we can now actually execute C code! A function kernel_main will be used as the entry
    #point to the OS
    call kernel_main

    #in case that function returns, we want to put the computer into an infinite loop
    #to do so, use clear interrupt instruction to disable interrupts,
    #the halt instruction to stop the CPU until the next interrupt arrives,
    #and jumping to the halt instruction if it ever continues execution, just to be safe.
    #let's create a local label rather than a real symbol and jump to there endlessly
    cli
    hlt
.Lhang:
    jmp .Lhang

#set size of the _start symbol to the current location '.' minus its start
#useful when debugging or whenever we implement call tracing
.size _start, . -_start






