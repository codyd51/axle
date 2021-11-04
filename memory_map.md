# x86_64

## User space

### Common mappings

`0x7e0000000000`: Stack
`0x7f0000000000`: Shared memory mappings
`0x7f8000000000`: AMC delivery pool

### Service-specific mappings

#### com.axle.awm

`0x7d0000000000`: Framebuffer

#### com.axle.file_manager

`0x7d0000000000`: initrd

## Kernel space (shared across every process)

`0xFFFF800000000000`: Physical RAM remapping
`0xFFFF900000000000`: Kernel heap
`0xFFFFFFFF80000000`: Kernel code/data
