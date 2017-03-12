#!/bin/bash
echo "Prefix: $PREFIX"
echo "Target (i686-elf recommended): $TARGET"
echo "Path: $PATH"
echo ""
echo "Please check that these are set correctly. If not, press any key to add them. Otherwise, type r"
read -n1 -rsp "" key
while [ "$key" != "r" ]; do
    read -n1 -r -p "" key
done
echo ""

arch=$(uname -m)
cwd=$(pwd)

# Get basic toold
echo "Verifing you have proper tools installed"
sleep 3
sudo apt-get install qemu libmpc-dev xorriso tmux curl \
	grub2 build-essential libstdc++6:i386 clang \
	git nasm

# Get prebuilt GCC
git clone https://github.com/rbheromax/i686-toolchain

cd $cwd

echo ""
echo "Compile Axle-OS now?: Press r to continue..."
read -n1 -rsp "" key
if [ "$key" = "r" ]; then
	make axle.iso CC=i686-toolchain/$arch/bin/i686-elf-gcc \
	CFLAGS="-g -ffreestanding -std=gnu99 -Wall -Wextra -I ./src" \
	LD=i686-toolchain/$arch/bin/i686-elf-ld \
	ISO_MAKER=grub-mkrescue

echo ""
echo "Type: 'make run' to open Axle-OS in Qemu"
else
cd $cwd
fi
echo ""
