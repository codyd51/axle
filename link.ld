/* the _start symbol is the kernel entry point */
ENTRY(_start)

SECTIONS {
	/* start dropping sections at 1mb */
	. = 1M;

	kernel_image_start = .;

	/* firstly, drop the multiboot header */
	.text BLOCK(4K) : ALIGN(4K) {
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

	kernel_image_end = .;
}
