<p align="center"><img width="285px" src="site/assets/axle.svg"/></p>

<p align="center"><img src="screenshots/doom.jpg"></p>

axle is a **UNIX-like** hobby operating system. Everything used within axle is **implemented from the ground up**, from the bootloader, to the window manager, to the assembler. axle runs on bare metal. axle provides a **desktop environment** via an efficient compositor and a homegrown GUI toolkit library.

[![2021 desktop environment](https://img.youtube.com/vi/Tg8nhEDbMOo/maxresdefault.jpg)](https://youtu.be/Tg8nhEDbMOo)
[![Assembler demo](https://img.youtube.com/vi/HhWE8ZvW4-g/maxresdefault.jpg)](https://youtu.be/HhWE8ZvW4-g)

<p align="center"><img src="screenshots/desktop1.png"></p>
<p align="center"><img src="screenshots/desktop2.png"></p>

Features (2021)
------------

* Compositing window manager with animations, alpha blending, and window clipping
* TCP/IP stack
* HTML/CSS rendering engine
* Userspace games like Snake, Breakout, DOOM (ported), and 2048
* Userspace applications like a web browser supporting HTTP
* MLFQ scheduler
* GUI toolkit
* Crash reporting
* Message-based IPC
* Driver interface
* ELF loader
* Newlib port
* Many supporting features (paging, ring3, syscalls, PCI, etc.)

Old Graphics (Up to 2018)
-------------

<p align="center"><img src="screenshots/help.png"></p>

## VGA graphics
Circles | Rectangles | Triangles | 
:------:|:----------:|:---------:
![Circles](/screenshots/circle.png) | ![Rectangles](/screenshots/rect.png) | ![Triangles](/screenshots/triangle.png) | 

Julia set | Mandelbrot set
:--------:|:-------------:
![Julia set](/screenshots/julia.png) | ![Mandelbrot set](/screenshots/mandelbrot.png)

## Old window manager (2018)

<p align="center"><img src="screenshots/text_test.png"></p>

Running
----------------------
axle's Github CI action serves as documentation for setting up an environment from scratch. 
axle uses QEMU as its standard emulator and runs on the real hardware I've tested. 

License
--------------
MIT License
