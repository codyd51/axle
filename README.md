axle
============================

axle is a small UNIX-like hobby operating system. It uses GRUB as its bootloader, but everything else is built up from scratch. We run C on 'bare metal', meaning we do not even have a standard library. Everything used is implemented within axle.

axle is interfaced through a shell. Experimental graphics support is implemented and used in VGA mode. 

The initial entry point must be done in ASM, as we have to do some special tasks such as setting up the GRUB header, pushing our stack, and calling our C entry point. This means that the first code run is in `boot.s`, but the 'real' entry point is in `kernel.c`.

Running
----------------------
To run and test axle on OS X, run `./install.sh` to install the necessary requirements, then `./run.sh` to start the emulator.

Features
----------------------

* Monolithic kernel
* Keyboard driver
* Timing from PIT
* Date driver from RTC
* Hardware interrupts 
* Paging
* Multicolored, scrolling shell
* Modified first-fit heap implementation
* Small standard library

Roadmap
---------------------

- [x] Keyboard driver
- [x] Hardware interrupts
- [ ] Snake!
- [ ] Software interrupts
- [x] Paging
- [x] Organize files
- [ ] Syscalls
- [ ] Multitasking
- [ ] User mode
- [ ] VESA, automatic resolution detection
- [x] GFX library
- [ ] Window manager
- [ ] Shutdown/reboot commands

Error Handling
-------------------

Interrupt service routines are dispatched based on the interrupt number. If an exception is caught, we do not attempt to recover from it. If the exception also sets an error code, we read it and print out the corresponding info it entails. We then hang until the machine is rebooted to prevent a triple fault.
