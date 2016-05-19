Install Guide for axle
============================

For testing with OS X, you will require an emulator. axle uses QEMU, which can be installed from homebrew (see http://brew.sh for installing homebrew):

```bash
brew install qemu
```

Since we are building for i686-elf, we need to cross compile grub for creating the axle iso. In order to cross compile grub, we need cross compiled versions of binutils, gcc and xorriso. Currently, binutils is version 2.26, gcc is version 6.1.0 and xorriso is 1.4.2.

First we need to install some tools before we can start compiling:
```bash
brew install gmp mpfr libmpc
```

And set some environment variables to make everything easier (add to your config):
```bash
export PREFIX="/path/to/tools/build"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
```

Now we can start compiling the tools:

First: binutils

```bash
mkdir tools
cd tools
mkdir build
curl -O http://ftp.gnu.org/gnu/binutils/binutils-2.26.tar.gz
tar -xvzf binutils-2.26.tar.gz
rm binutils-2.26.tar.gz
mkdir binutilsbuild
cd binutilsbuild
../binutils-2.26/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install
cd ../
```

Second: xorriso

```bash
brew install xorriso
```

Third: gcc (takes a while)

```bash
curl -O ftp://ftp.gnu.org/gnu/gcc/gcc-6.1.0/gcc-6.1.0.tar.bz2
tar -xvjf gcc-6.1.0.tar.bz2
rm gcc-6.1.0.tar.bz2
mkdir gccbuild
cd gccbuild
../gcc-6.1.0/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc
cd ../
```

Finally, let's build grub:

```bash
git clone git://git.savannah.gnu.org/grub.git
git clone https://github.com/vertis/objconv.git
cd objconv
g++ -o objconv -O2 src/*.cpp
mv objconv /usr/local/bin/
cd ../grub
./autogen.sh
cd ../
mkdir grubbuild
cd grubbuild
../grub/configure --disable-werror TARGET_CC=$TARGET-gcc TARGET_OBJCOPY=$TARGET-objcopy TARGET_STRIP=$TARGET-strip TARGET_NM=$TARGET-nm TARGET_RANLIB=$TARGET-ranlib TARGET_XORRISO=$TARGET-xorriso --target=$TARGET
make
make install
```

Now you can run `./run.sh`!
