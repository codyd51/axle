#!/usr/local/bin/python3
"""Install public kernel headers into /usr/include/
These are functions available via syscalls to user-mode programs.
"""
import shutil
from pathlib import Path
from build_utils import sysroot_copy_needs_update
from distutils.dir_util import copy_tree


def copy_kernel_headers():
    src_root = Path(__file__).parent / "kernel"
    sysroot = Path(__file__).parent / "axle-sysroot"
    include_dir = sysroot / "usr" / "i686-axle" / "include"
    newlib_axle_root = Path(__file__).parent / "ports" / "newlib" / "newlib-2.5.0.20171222" / "newlib" / "libc" / "sys" / "axle"

    headers_to_copy = [
        # Copy kernel headers to the sysroot
        (src_root / "kernel" / "util" / "amc" / "amc.h", include_dir / "kernel" / "amc.h"),
        (src_root / "kernel" / "util" / "amc" / "core_commands.h", include_dir / "kernel" / "core_commands.h"),
        (src_root / "kernel" / "util" / "adi" / "adi.h", include_dir / "kernel" / "adi.h"),
        (src_root / "kernel" / "interrupts" / "idt.h", include_dir / "kernel" / "idt.h"),
        # Copy kernel headers from the sysroot to the newlib port
        (include_dir / "kernel", newlib_axle_root / "include" / "kernel"),
        # Copy stdlib additions from newlib port to the sysroot
        (newlib_axle_root / "array.h", include_dir / "stdlibadd" / "array.h"),
        (newlib_axle_root / "assert.h", include_dir / "stdlibadd" / "assert.h"),
        (newlib_axle_root / "sleep.h", include_dir / "stdlibadd" / "sleep.h"),
    ]

    for source_path, include_path in headers_to_copy:
        # If the files are identical, no need to copy
        if sysroot_copy_needs_update(source_path, include_path):
            print(f'Copying kernel source tree {source_path} to sysroot path {include_path}')
            include_path.parent.mkdir(exist_ok=True, parents=True)
            if source_path.is_dir():
                copy_tree(source_path.as_posix(), include_path.as_posix())
            else:
                shutil.copy(source_path.as_posix(), include_path.as_posix())


if __name__ == '__main__':
    copy_kernel_headers()