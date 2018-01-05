/* entry point of the kernel */
ENTRY(_start)

SECTIONS {
    /* start placing sections at 1mb */
    . = 1M;

    _kernel_image_start = .;

    .text BLOCK(4K) : ALIGN(4K) {
        /* as per the standard, multiboot header must be within first 8kb */
        *(.multiboot_header)
		*(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }

    _kernel_image_end = .;
}
