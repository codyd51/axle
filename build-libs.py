#!/usr/local/bin/python3
"""Rebuild the user-space amc library (libamc.a)
and install to axle-sysroot/usr/i686-axle/lib/libamc.a
"""
import shutil
import subprocess
import pathlib


def main():
    libdir_to_libname = {
        "agx": "libagx.a",
        "libamc": "libamc.a",
        "libport": "libport.a"
    }

    for library_dirname, build_product_name in libdir_to_libname.items():
        library_source_dir = pathlib.Path(__file__).parent / "src" / "user" / "extern" / library_dirname
        sysroot = pathlib.Path(__file__).parent / "axle-sysroot"

        status = subprocess.run("make", cwd=library_source_dir.as_posix(), shell=True)
        if status.returncode != 0:
            raise RuntimeError(f"Make failed with exit code {status.returncode}: {status.stdout} {status.stderr}")

        # Copy the build product to /usr/lib
        library_build_product = library_source_dir / build_product_name
        assert(library_build_product.exists())
        library_dest = sysroot / "usr" / "i686-axle" / "lib" / build_product_name
        shutil.copy(library_build_product.as_posix(), library_dest.as_posix())
        assert(library_dest.exists())

        # Copy the headers to /usr/include
        headers_dest_dir = sysroot / "usr" / "i686-axle" / "include" / library_dirname
        for absolute_source_header in library_source_dir.rglob('*.h'):
            relative_source = absolute_source_header.relative_to(library_source_dir)
            dest_header = headers_dest_dir / relative_source
            print(f'{build_product_name}:\tCopy header {relative_source}')

            shutil.copy(absolute_source_header, dest_header)


if __name__ == '__main__':
    main()