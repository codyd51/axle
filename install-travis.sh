#!/bin/bash
echo "Prefix: $PREFIX"
echo "Target (i686-elf recommended): $TARGET"
echo "Path: $PATH"
echo ""

arch="$(uname -m)"
cwd="$(pwd)"

# Get basic tools
echo "Verifing you have proper tools installed"
if command -v apt-get >/dev/null 2>&1; then
	sudo apt-get install qemu libmpc-dev xorriso tmux curl \
	grub2 build-essential libstdc++6:i386 clang \
	git nasm
else
	echo "You need to have apt-get installed to auto-install the tools."
	echo "Make sure you have the needed tools installed, then type r."
	echo "Needed tools: qemu libmpc-dev xorriso tmux curl grub2 build-essential libstdc++6:i386 clang git nasm"
	# Wait for input.
	read -n1 -rsp "" key
	while [ "$key" != "r" ]; do
    		read -n1 -r -p "" key
	done
fi

# Get prebuilt GCC
git clone https://github.com/rbheromax/i686-toolchain

cd "$cwd"

echo ""
	sudo make axle.iso CC=i686-toolchain/$arch/bin/i686-elf-gcc \
	CFLAGS="-g -ffreestanding -std=gnu99 -Wall -Wextra -I ./src" \
	LD=i686-toolchain/$arch/bin/i686-elf-ld \
	ISO_MAKER=grub-mkrescue

echo ""
echo "Type: 'make run' to open Axle-OS in Qemu"
