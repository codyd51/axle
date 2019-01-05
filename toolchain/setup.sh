#!/bin/bash
export DEBIAN_FRONTEND=noninteractive

echo Configuring system...
apt-get update >/dev/null
apt-get install ca-certificates -y --no-install-recommends --quiet >/dev/null

echo Installing dependencies...
apt-get install qemu libmpc-dev xorriso tmux curl grub2 build-essential clang git nasm -y --no-install-recommends --quiet >/dev/null

echo Getting toolchain...
cd /
rm -rf i686-toolchain/ >/dev/null
git clone https://github.com/rbheromax/i686-toolchain -q >/dev/null

echo Environment setup!
cd ..