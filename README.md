axle
============================

        |       
:------:|:------:
![Boot logo](/screenshots/boot.png) | ![Startup](/screenshots/startup.png)

axle is a small UNIX-like hobby operating system. It uses GRUB as its bootloader, but everything else is built up from scratch. We run C on 'bare metal', meaning we do not even have a standard library. Everything used is implemented within axle.

axle is interfaced through a shell. VGA and higher-resolution VESA drivers are available, along with a small graphics library supporting both modes. A small window manager is also provided.

![Shell](/screenshots/help.png)

#VGA graphics
Circles | Rectangles | Triangles | Julia set | Mandelbrot set
:------:|:----------:|:---------:|:---------:|:-------------:
![Circles](/screenshots/circle.png) | ![Rectangles](/screenshots/rect.png) | ![Triangles](/screenshots/triangle.png) | ![Julia set](/screenshots/julia.png) | ![Mandelbrot set](/screenshots/mandelbrot.png)

#VESA-mode window manager
        |              
:------:|:------:
![Colors](/screenshots/color_test.png) | ![Text](/screenshots/text_test.png)

The initial entry point must be done in ASM, as we have to do some special tasks such as setting up the GRUB header, pushing our stack, and calling our C entry point. This means that the first code run is in `boot.s`, but the 'real' entry point is in `kernel.c`.

Running
----------------------
To run and test axle on OS X, run `./install.sh` to install the necessary requirements, then `make run` to start the emulator.

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
* Multitasking
* User mode
* Syscalls

Roadmap
---------------------

- [x] Keyboard driver
- [x] Hardware interrupts
- [ ] Snake!
- [x] Software interrupts
- [x] Paging
- [x] Organize files
- [x] Syscalls
- [x] Multitasking
- [x] User mode
- [x] VESA
- [ ] Automatic resolution detection
- [x] GFX library
- [ ] Window manager
- [x] Shutdown/reboot commands

Error Handling
-------------------

Interrupt service routines are dispatched based on the interrupt number. If an exception is caught, we do not attempt to recover from it. If the exception also sets an error code, we read it and print out the corresponding info it entails. We then hang until the machine is rebooted to prevent a triple fault.
