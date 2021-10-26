/* Kernel entry point */
ENTRY(_start)

/* Kernel is loaded at the top of the 64-bit address space minus 2G */
KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS {
    . = KERNEL_VMA;

    _kernel_image_start = .;

    .text BLOCK(4K) : ALIGN(4K) {
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
