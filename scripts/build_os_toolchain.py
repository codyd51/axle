#!/usr/bin/python3
import os
import shutil
import tempfile
from pathlib import Path
from typing import Tuple

from build_utils import download_and_unpack_archive, run_and_check


def clone_tool_and_prepare_build_dir(build_dir: Path, url: str) -> Tuple[Path, Path]:
    tool_src_dir = download_and_unpack_archive(build_dir, url)
    tool_name = url.split("/")[-1].removesuffix(".tar.gz")
    tool_build_dir = build_dir / f"build-{tool_name}"
    tool_build_dir.mkdir(exist_ok=True)
    return tool_src_dir, tool_build_dir


def sleep():
    import time
    while True:
        time.sleep(1)


def install_dependencies():
    print(f"Installing dependencies...")
    run_and_check(["sudo", "apt", "update"])
    dependencies = [
        "build-essential",
        "bison",
        "flex",
        "libgmp3-dev",
        "libmpc-dev",
        "libmpfr-dev",
        "texinfo",
        "xorriso",
    ]
    run_and_check(["sudo", "apt", "install", "-y", *dependencies])


def build() -> None:
    axle_dir = Path(__file__).parents[1]
    sysroot_dir = axle_dir / "axle-sysroot"
    sysroot_dir.mkdir(exist_ok=True)
    arch = "x86_64"
    arch_target = f"{arch}-elf"
    toolchain_dir = axle_dir / f"{arch}-toolchain"
    toolchain_dir.mkdir(exist_ok=True)
    binaries_dir = toolchain_dir / "bin"

    build_products_dir = Path(__file__).parents[1] / f"{arch}-toolchain"
    build_products_dir.mkdir(exist_ok=True)

    with tempfile.TemporaryDirectory() as build_dir_raw:
        build_dir = Path(build_dir_raw)
        intermediate_build_products_dir = build_dir / "intermediate-build-products"

        # GCC requires the libc headers to be present in the sysroot before it can build
        # But, we need some kind of cross-compiler to build libc before we can 'properly' install the libc headers to the sysroot
        # We have two options:
        #   1. Multiple stages 
        #       a. Build an initial generic x86_64-elf cross-compiler
        #       b. Use it to build libc, headers are installed to sysroot
        #       c. Continue with building x86_64-axle cross-compiler
        #       d. Rebuild the libc with the x86_64-axle cross-compiler (perhaps not necessary?)
        #   2. Copy headers
        #       a. Copy the libc headers directly from the libc source tree into the sysroot
        #       b. Build the x86_64-axle cross-compiler
        #       c. Build the libc with the x86_64-axle cross-compiler
        # In this script we do approach #2
        #
        # As per the above comments, copy the libc headers to the sysroot without actually building the libc
        # Copy headers from newlib-2.5.../newlib/libc/include/
        newlib_src_dir, newlib_build_dir = clone_tool_and_prepare_build_dir(build_dir, "ftp://sourceware.org/pub/newlib/newlib-2.5.0.20171222.tar.gz")
        libc_source_headers_dir = newlib_src_dir / 'newlib' / 'libc' / 'include'
        sysroot_libc_headers_dir = sysroot_dir / 'usr' / 'include'
        sysroot_libc_headers_dir.mkdir(parents=True, exist_ok=True)
        shutil.copytree(libc_source_headers_dir.as_posix(), sysroot_libc_headers_dir.as_posix(), dirs_exist_ok=True)

        # Build automake
        automake_src_dir, automake_build_dir = clone_tool_and_prepare_build_dir(
            build_dir, "https://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.gz"
        )
        automake_configure_path = automake_src_dir / "configure"
        run_and_check(
            [automake_configure_path.as_posix(), f"--prefix={intermediate_build_products_dir}"], cwd=automake_build_dir
        )
        run_and_check(["make"], cwd=automake_build_dir)
        run_and_check(["make", "install"], cwd=automake_build_dir)

        # Build autoconf
        autoconf_src_dir, autoconf_build_dir = clone_tool_and_prepare_build_dir(
            build_dir, "https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz"
        )
        autoconf_configure_path = autoconf_src_dir / "configure"
        run_and_check(
            [autoconf_configure_path.as_posix(), f"--prefix={intermediate_build_products_dir}"], cwd=autoconf_build_dir
        )
        run_and_check(["make"], cwd=autoconf_build_dir)
        run_and_check(["make", "install"], cwd=autoconf_build_dir)

        # Build binutils
        binutils_url = "https://ftp.gnu.org/gnu/binutils/binutils-2.37.tar.gz"
        binutils_dir = download_and_unpack_archive(build_dir, binutils_url)
        binutils_build_dir = build_dir / "build-binutils"
        binutils_build_dir.mkdir(exist_ok=True)
    
        binutils_patch_file = Path(__file__).parent / "binutils.patch"

        run_and_check(['git', 'apply', '--check', binutils_patch_file.as_posix()], cwd=binutils_dir)
        run_and_check(['git', 'apply', binutils_patch_file.as_posix()], cwd=binutils_dir)

        # Add the autotools we just built to PATH
        env = {"PATH": f'{(intermediate_build_products_dir / "bin").as_posix()}:{os.environ["PATH"]}'}

        # We need to run automake in the ld directory, since our patch modifies Makefile.am
        ld_dir = binutils_dir / 'ld'
        run_and_check(['automake'], cwd=ld_dir, env_additions=env)

        configure_script_path = binutils_dir / "configure"
        run_and_check(
            [
                configure_script_path.as_posix(),
                f"--target=x86_64-elf-axle",
                f"--prefix={build_products_dir.as_posix()}",
                f"--with-sysroot={sysroot_dir}",
                "--disable-nls",
                "--disable-werror",
            ],
            cwd=binutils_build_dir,
        )
        run_and_check(["make", '-j', '8'], cwd=binutils_build_dir)
        run_and_check(["make", "install"], cwd=binutils_build_dir)

        gcc_url = "https://ftp.gnu.org/gnu/gcc/gcc-11.2.0/gcc-11.2.0.tar.gz"
        gcc_dir = download_and_unpack_archive(build_dir, gcc_url)
        gcc_build_dir = build_dir / "build-gcc"
        gcc_build_dir.mkdir(exist_ok=True)
        configure_script_path = gcc_dir / "configure"

        gcc_patch_file = Path(__file__).parent / "gcc.patch"

        run_and_check(['git', 'apply', '--check', gcc_patch_file.as_posix()], cwd=gcc_dir)
        run_and_check(['git', 'apply', gcc_patch_file.as_posix()], cwd=gcc_dir)

        # We need to run autoconf in the libstdc++ directory, since our patch modifies a .m4 input file
        libstdcpp_dir = gcc_dir / 'libstdc++-v3'
        run_and_check(['autoconf'], cwd=libstdcpp_dir, env_additions=env)

        run_and_check(
            [
                configure_script_path.as_posix(),
                f"--target=x86_64-elf-axle",
                f"--prefix={build_products_dir.as_posix()}",
                f"--with-sysroot={sysroot_dir.as_posix()}",
                "--disable-nls",
                # Ref: https://stackoverflow.com/questions/46487529/crosscompiling-gcc-link-tests-are-not-allowed-after-gcc-no-executables-when-che
                "--disable-bootstrap",
                "--enable-languages=c,c++",
            ],
            cwd=gcc_build_dir,
            env_additions=env
        )
        run_and_check(["make", "all-gcc", "-j", "12"], cwd=gcc_build_dir)
        run_and_check(["make", "all-target-libgcc"], cwd=gcc_build_dir, env_additions=env)

        run_and_check(["make", "install-gcc"], cwd=gcc_build_dir)
        run_and_check(["make", "install-target-libgcc"], cwd=gcc_build_dir)

        # Build the libc (newlib) before continuing to building libstdc++-v3
        import build_newlib
        build_newlib.build()

        try:
            run_and_check(['make', 'all-target-libstdc++-v3'], cwd=gcc_build_dir)
        except Exception as e:
            print(e)
            print(gcc_build_dir)
            print(gcc_dir)
            sleep()
        run_and_check(['make', 'install-target-libstdc++-v3'], cwd=gcc_build_dir)


if __name__ == "__main__":
    install_dependencies()
    build()
