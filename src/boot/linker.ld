/*  the bootloader will look at this image and start execution at the symbol
    designated by the entry point
*/
ENTRY(start)

/*  tell where the various sections of the object files will be put in the
    final kernel image
*/
SECTIONS {
    /*  begin putting sections at 1Mib, a conventional place for
        kernels to be loaded at by the bootloader
    */
    . = 1M;

    /*  first put the multiboot header, as it's required to be put very early in the
        image or the bootloader won't recognize the file format
        then we'll put the .text section
    */
    .text ALIGN(4K) : {
        *(.multiboot)
	code = .; _code = .; __code = .;
        *(.mboot)
	*(.text)
	. = ALIGN(4096);
    }

    /*  read-only data
    *
    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }*/

    /*  read-write data (initialized)
    */
    .data : {
	data = .; _data = .; __data = .;
        *(.data)
	*(.rodata)
	. = ALIGN(4096);
    }

    /*  read-write data (uninitialized) and stack
    */
    .bss : {
	bss = .; _bss = .; __bss = .;
        *(COMMON)
        *(.bss)
        *(.bootstrap_stack)
	. = ALIGN(4096);
    }

    end = .; _end = .; __end = .;

    /*  the compiler may produce other sections, but by default it will put them
        in a segment with the same name.
        let's just add stuff here as needed
    */
}
