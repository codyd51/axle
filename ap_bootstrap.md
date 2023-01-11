# AP bootstrap

## Background

In a multicore environment, different processor cores fit into one of two designations:

Bootstrap processor (BSP): The CPU core selected as the 'initial' running CPU by the firmware/platform startup code, prior to the OS bootloader.

Application processor (AP): Any CPU core that is not the BSP.

When the OS code running on the BSP has sufficiently initialized the OS environment, it is the BSP's responsibility to boot and bring up the APs. 

This involves a few steps (at a high level):

- Parse the ACPI tables to find the APs
- Enable the APIC within each AP
- Send a series of init messages to wake up each AP

When these steps are performed, the APs will start executing code at some OS-specified page boundary below 1MB. 

The APs will start up in Real Mode, and it's the OS's responsibility to transition them from Real Mode, to Protected Mode, to Long Mode.

## Approach

We need a consistent location to point the APs to. This location needs to contain code that expects to be loaded there. 

This is difficult to achieve without special work: while we can specify any load location we like in linker scripts, 

the bootloader will interpret these load locations as virtual addresses, and we don't have sufficient control in 

UEFI-land to guarantee our desired physical address allocation. 

Therefore, the build system now specially compiles a piece of assembly and includes the compiled artifact in the UEFI FS image. 

This module expects a specific load location, defined in `kernel/ap_bootstrap.h`.

The bootloader will map this artifact into memory somewhere, and will inform the kernel of its location. 

The kernel's PMM will reserve the desired location, and will eventually copy the artifact from its loaded location to its desired location.

This artifact will perform the work of transitioning from Real Mode, to Protected Mode, to Long Mode. 

However, transitioning between these modes and setting up the AP requires several data structures to be ready to go (non-exhaustive):

- Protected Mode GDT
- Long Mode GDT
- IDT
- Kernel stack

Therefore, we reserve an extra page adjacent to the page containing the bootstrap code: the kernel will map 

the parameters expected by the bootstrap program in well-defined offsets in the parameters page, and the AP bootstrap program will know where to look for them from. 
