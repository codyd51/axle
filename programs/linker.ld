SEARCH_DIR(/Users/philliptennen/Documents/develop/axle/axle-sysroot/usr/x86_64-axle/lib)
STARTUP(crt0.o)
ENTRY(_start)
GROUP(
   libg.a
   libc.a
   libm.a
   libnosys.a
 )

SECTIONS {
    . = 4M;

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
}
