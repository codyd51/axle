<h1 align="center">axle OS</h1>

<p align="center"><img src="screenshots/boot.png"></p>

axle is a small **UNIX-like** hobby operating system. Everything used within axle is **implemented from the ground up**, aside from the bootloader, for which we use GRUB. axle is a **multiboot compliant** kernel. axle runs C on 'bare metal' in freestanding mode, meaning even the C standard library is not included. A subset of the C standard library is implemented within axle's kernel, and a userspace is provided through a **Newlib port**. axle is mainly **interfaced through a shell**.

<p align="center"><img src="screenshots/startup.png"></p>

The initial entry point must be done in ASM, as we have to do some special tasks such as setting up the GRUB header, pushing our stack, and calling our C entry point. This means that the first code run is in `boot.s`, but the 'real' entry point is in `kernel.c`.

Features
------------
* Compositing window manager with clipping and alpha blending
* Text renderer with SSAA antialiasing
* Auto-boots in 1024x768 full RGB
* ELF loader
* Newlib port
* MLFQ scheduler
* PS/2 keyboard/mouse drivers
* PIT/RTC drivers
* Various graphics modes/resolutions
* Graphics library
* Paging
* Multicolored, scrolling shell with history
* Kernel-space standard library
* User mode (ring3)
* Syscalls
* PCI enumeration

Overview
-------------
axle is designed so everything is preemptible, including the kernel itself. Tasks are scheduled according to an MLFQ policy, although a round-robin implementation is also provided if the user requires low-latency between tasks. To prevent starvation in MLFQ mode, a priority booster is also included which drops every task into the highest priority queue at regular intervals. The scheduler quantum, boost quantum, queue counts and queue sizes are all configurable, as many constants within axle's kernel are.

axle's kernel begins by boostrapping essential facilities, such as descriptor tables, paging, the PIT driver (a neccesary component of many other mechanisms), multitasking, and more. A shell process is then launched, which supports many commands for interfacing with axle. Additionally, the shell supports backspacing over commands and automatically records all history (both input and output) to support scrolling back through previous messages.

axle can load ELF processes from the virtual filesystem, and, recently, the main shell itself is no longer part of the kernel. and is a user-mode process. ELFs can utilize axle's newlib port as a standard library implementation.

Among axle's built-in demonstrations is rexle, a pseudo-3D renderer which utilizes raycasting. Rexle paints walls with textures loaded from axle's virtual filesystem, represented as BMPs. BMPs can also be rendered in axle's desktop environment, so that provides a background and image viewer.

axle also includes a JIT compiler which is capable of executing assembly as it is entered. (At the present moment it accepts opcodes directly, though the translation from assembly to opcodes is not a domain-specific problem, and you can feel free to use your favorite assembler to do so).

Graphics
-------------

While axle is mainly used through a terminal, VGA and higher-resolution VESA drivers are available, along with a graphics library which can be used with both modes. A window manager and associated API is also provided. VGA mode supports 256 colors and VESA supports full RGB.

<p align="center"><img src="screenshots/help.png"></p>

##VGA graphics
Circles | Rectangles | Triangles | 
:------:|:----------:|:---------:
![Circles](/screenshots/circle.png) | ![Rectangles](/screenshots/rect.png) | ![Triangles](/screenshots/triangle.png) | 

Julia set | Mandelbrot set
:--------:|:-------------:
![Julia set](/screenshots/julia.png) | ![Mandelbrot set](/screenshots/mandelbrot.png)

##Window manager

While in previous versions of axle the window manager refresh rate was implemented as a callback from the PIT, as axle is now capable of multitasking this is no longer necessary. The window manager spawns its own process and refreshes its contents whenever necessary. Upon a refresh, the window manager traverses its heirarchy and draws its contents from the bottom up, writes to a temporary buffer, and decides whether to transfer this buffer to real video memory. The window manager attempts to minimize modifying video memory, and only does so if there has been a change from the current frame. Additionally, the buffer itself is not modified if no UI elements have notified the manager that they need to be redrawn.

axle's window manager exposes an API for creating and managing UI elements such as windows, views, labels, BMPs, and more. In VESA mode, full RGB is supported.

axle includes a text renderer and default 8x8 bitmap font, though any font in this format could be trivially loaded from axle's filesystem.
<p align="center"><img src="screenshots/text_test.png"></p>

Running
----------------------
Unless your platform natively outputs 32-bit x86 binaries, you will need a cross compiler to build axle. [This link](http://wiki.osdev.org/GCC_Cross-Compiler) provides detailed instructions on how to cross-compile GCC to build suitable binaries. Alternatively, a precompiled toolchain can be downloaded [here](https://github.com/mstg/i686-toolchain).
axle uses QEMU as its standard emulator, though any other could be used, such as Bochs. To modify this and other build parameters, see the `Makefile`.
To run and test axle on OS X, run `./install.sh` to attempt to build the toolchain, then `make run` to start the emulator.

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
- [x] Window manager
- [x] Shutdown/reboot commands
- [ ] Modifiable filesystem
- [ ] Load external binaries
- [x] Thread API
- [ ] Polygon support 
- [ ] UI toolkit
- [x] MLFQ scheduling
- [x] Variable scheduling modes
- [x] PCI enumeration
- [ ] E1000 network driver

License
--------------
MIT license. axle is provided free of charge for use or learning purposes. If you create a derivative work based on axle, please provide attribution to the original project.
