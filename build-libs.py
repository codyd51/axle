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
        "libport": "libport.a",
        "libnet": "libnet.a",
        "libgui": "libgui.a",
        "libimg": "libimg.a",
    }

    for library_dirname, build_product_name in libdir_to_libname.items():
        library_source_dir = pathlib.Path(__file__).parent / "src" / "user" / "extern" / library_dirname
        sysroot = pathlib.Path(__file__).parent / "axle-sysroot"

        print(f"Build {build_product_name} in {library_source_dir}")
        status = subprocess.run("make", cwd=library_source_dir.as_posix(), shell=True, stdout=subprocess.PIPE)
        if status.returncode != 0:
            raise RuntimeError(f"Make failed with exit code {status.returncode}: {status.stdout} {status.stderr}")
        if "is up to date" in status.stdout.decode():
            # No need to copy build products
            print(f'Skip copying build products and headers, {build_product_name} is up to date')
            continue

        # Copy the build product to /usr/lib
        library_build_product = library_source_dir / build_product_name
        assert(library_build_product.exists())
        library_dest = sysroot / "usr" / "i686-axle" / "lib" / build_product_name
        shutil.copy(library_build_product.as_posix(), library_dest.as_posix())
        assert(library_dest.exists())
        print(f'\t{library_dirname}: Copy build product {build_product_name} -> {library_dest}')

        # Copy the headers to /usr/include
        headers_dest_dir = sysroot / "usr" / "i686-axle" / "include" / library_dirname
        for absolute_source_header in library_source_dir.rglob('*.h'):
            relative_source = absolute_source_header.relative_to(library_source_dir)
            dest_header = headers_dest_dir / relative_source
            print(f'{build_product_name}:\tCopy header {relative_source}')

            shutil.copy(absolute_source_header, dest_header)


if __name__ == '__main__':
    main()