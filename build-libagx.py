#!/usr/local/bin/python3
"""Rebuild the user-space graphics library (libagx.a) 
and install to axle-sysroot/usr/i686-axle/lib/libagx.a
"""
import shutil
import subprocess
import pathlib


def main():
    agx_source_dir = pathlib.Path(__file__).parent / "src" / "user" / "extern" / "agx"
    sysroot = pathlib.Path(__file__).parent / "axle-sysroot"

    status = subprocess.run("make", cwd=agx_source_dir.as_posix(), shell=True)
    if status.returncode != 0:
        raise RuntimeError(f"Make failed with exit code {status.returncode}: {status.stdout} {status.stderr}")

    # Move the build product to /usr/lib
    libagx_build_product = agx_source_dir / "libagx.a"
    assert(libagx_build_product.exists())
    libagx_dest = sysroot / "usr" / "i686-axle" / "lib" / "libagx.a"
    shutil.move(libagx_build_product.as_posix(), libagx_dest.as_posix())
    assert(libagx_dest.exists())

    # Copy the headers to /usr/include
    headers_dest_dir = sysroot / "usr" / "i686-axle" / "include" / "agx"
    for absolute_source_header in agx_source_dir.rglob('*.h'):
        relative_source = absolute_source_header.relative_to(agx_source_dir)
        dest_header = headers_dest_dir / relative_source
        shutil.copy(absolute_source_header, dest_header)


if __name__ == '__main__':
    main()