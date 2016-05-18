Install Guide for axle
============================

For testing with OS X, you will require an emulator. axle uses QEMU, which can be installed from homebrew (see http://brew.sh for installing homebrew):

```bash
brew install qemu
```

Since we are building for i686-elf, we need to cross compile grub for creating the axle iso. In order to cross compile grub, we need cross compiled versions of binutils, gcc and xorriso. Currently, binutils is version 2.26, gcc is version 6.1.0 and xorriso is 1.4.2.


(write about how you need apple-gcc42 from homebrew/dupes)

```bash
brew install homebrew/dupes/apple-gcc42
```

Now we can start compiling the tools:

```bash

```