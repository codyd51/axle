#!/usr/local/bin/python3
"""Install public kernel headers into /usr/include/
These are functions available via syscalls to user-mode programs.
"""
import os
import shutil
from pathlib import Path


def sysroot_copy_needs_update(source_path: Path, sysroot_path: Path) -> bool:
    return os.stat(source_path.as_posix()).st_mtime - os.stat(sysroot_path.as_posix()).st_mtime > 1


def copy_kernel_headers():
    src_root = Path(__file__).parent / "src"
    sysroot = Path(__file__).parent / "axle-sysroot"
    include_dir = sysroot / "usr" / "i686-axle" / "include"

    headers_to_copy = {
        src_root / "kernel" / "util" / "amc" / "amc.h": include_dir / "kernel" / "amc.h",
        src_root / "kernel" / "util" / "amc" / "core_commands.h": include_dir / "kernel" / "core_commands.h",
        src_root / "kernel" / "util" / "adi" / "adi.h": include_dir / "kernel" / "adi.h",
        src_root / "kernel" / "interrupts" / "idt.h": include_dir / "kernel" / "idt.h",
    }

    for source_path, include_path in headers_to_copy.items():
        # If the files are identical, no need to copy
        if sysroot_copy_needs_update(source_path, include_path):
            print(f'Copying kernel source tree {source_path} to sysroot path {include_path}')
            shutil.copy(source_path, include_path)


if __name__ == '__main__':
    copy_kernel_headers()