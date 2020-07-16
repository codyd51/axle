#!/usr/local/bin/python3
"""Install public kernel headers into /usr/include/
These are functions available via syscalls to user-mode programs.
"""
import shutil
import subprocess
import pathlib


def main():
    src_root = pathlib.Path(__file__).parent / "src"
    sysroot = pathlib.Path(__file__).parent / "axle-sysroot"
    include_dir = sysroot / "usr" / "i686-axle" / "include"

    headers_to_copy = {
        src_root / "kernel" / "util" / "amc" / "amc.h": include_dir / "kernel" / "amc.h"
    }

    for source_path, include_path in headers_to_copy.items():
        shutil.copy(source_path, include_path)


if __name__ == '__main__':
    main()