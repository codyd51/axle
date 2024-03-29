#!/usr/local/bin/python3
"""Install public kernel headers into /usr/include/
These are functions available via syscalls to user-mode programs.
"""
import shutil
from distutils.dir_util import copy_tree
from pathlib import Path

from build_utils import copied_file_is_outdated


def copy_kernel_headers():
    src_root = Path(__file__).parents[1] / "kernel"
    bootloader_root = Path(__file__).parents[1] / "bootloader"
    sysroot = Path(__file__).parents[1] / "axle-sysroot"
    include_dir = sysroot / "usr" / "include"

    headers_to_copy = [
        # Copy kernel headers to the sysroot
        (src_root / "kernel" / "util" / "amc" / "amc.h", include_dir / "kernel" / "amc.h"),
        (src_root / "kernel" / "util" / "amc" / "core_commands.h", include_dir / "kernel" / "core_commands.h"),
        (src_root / "kernel" / "util" / "adi" / "adi.h", include_dir / "kernel" / "adi.h"),
        (src_root / "kernel" / "interrupts" / "idt.h", include_dir / "kernel" / "idt.h"),
        # Copy bootloader header to the sysroot
        (bootloader_root / "axle_boot_info.h", include_dir / "bootloader" / "axle_boot_info.h"),
    ]

    for source_path, include_path in headers_to_copy:
        include_path.parent.mkdir(exist_ok=True, parents=True)
        # If the files are identical, no need to copy
        if not include_path.exists() or copied_file_is_outdated(source_path, include_path):
            print(f"Copying kernel source tree {source_path} to sysroot path {include_path}")
            if source_path.is_dir():
                copy_tree(source_path.as_posix(), include_path.as_posix())
            else:
                shutil.copy(source_path.as_posix(), include_path.as_posix())


if __name__ == "__main__":
    copy_kernel_headers()
