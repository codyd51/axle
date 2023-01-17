# x86_64

## Physical memory

`0x8000`: AP bootstrap code page
`0x9000`: AP bootstrap data page

As APs start up in real mode, the first code they execute must be within the first 1MB of physical address space.

Therefore, we need a mechanism by which we can guarantee the location of the AP bootstrap program, so we can tell APs where to boot from. 

The approach I've taken is:

- An arbitrary low page (`0x8000`) is selected to be used for the AP bootstrap program.
- The AP bootstrap program is compiled as part of axle's build system, and is copied to the UEFI filesystem. 
- axle's bootloader will load the program off the UEFI filesystem into somewhere in memory, and will inform the kernel of its location.
- The kernel's PMM is aware of the AP bootstrap page, and will not push it to the allocatable pool.
- The kernel copies the bootstrap program from where it was loaded by the bootloader to the dedicated page.
- SIPIs are sent to APs that point them to boot from the AP bootstrap page. 
- Another page (`0x9000`) is used to store parameters to the AP bootstrap program, such as the GDT that should be used to enter protected mode.

## Virtual memory

### User space

#### Common mappings

`0x7e0000000000`: Stack
`0x7f0000000000`: Shared memory mappings
`0x7f8000000000`: AMC delivery pool

#### Service-specific mappings

##### com.axle.awm

`0x7d0000000000`: Framebuffer

##### com.axle.file_manager

`0x7d0000000000`: initrd

##### Any user of `AMC_MAP_PHYSICAL_RANGE_REQUEST`

`0x7d0000000000`: Virtual base of mapped physical memory

### Kernel space (unique per CPU core)

`0xFFFFA00000000000`: Per-CPU core kernel data

### Kernel space (shared across every process)

`0xFFFF800000000000`: Physical RAM remapping
`0xFFFF900000000000`: Kernel heap
`0xFFFFFFFF80000000`: Kernel code/data
