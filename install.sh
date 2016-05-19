brew install qemu gmp mpfr libmpc xorriso
mkdir tools
cd tools
curl -O http://ftp.gnu.org/gnu/binutils/binutils-2.26.tar.gz
tar -xvzf binutils-2.26.tar.gz
rm binutils-2.26.tar.gz
mkdir binutilsbuild
cd binutilsbuild
../binutils-2.26/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install
cd ../
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
cd ../../
