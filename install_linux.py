#!/usr/bin/python3
import os
import tempfile
from pathlib import Path

from build_utils import download_and_unpack_archive, run_and_check


def install() -> None:
    print(f"Installing dependencies...")
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
    # run_and_check(["sudo", "apt", "install", *dependencies])

    axle_dir = Path(__file__).parent
    arch_target = "i686-elf"
    toolchain_dir = axle_dir / "i686-toolchain"

    with tempfile.TemporaryDirectory() as build_dir_raw:
        build_dir = Path(build_dir_raw)

        if False:
            binutils_url = "https://ftp.gnu.org/gnu/binutils/binutils-2.37.tar.gz"
            binutils_dir = download_and_unpack_archive(build_dir, binutils_url)
            binutils_build_dir = build_dir / "build-binutils"
            binutils_build_dir.mkdir(exist_ok=True)
            configure_script_path = binutils_dir / "configure"
            run_and_check(
                [
                    configure_script_path.as_posix(),
                    f"--target={arch_target}",
                    f"--prefix={toolchain_dir.as_posix()}",
                    "--with-sysroot",
                    "--disable-nls",
                    "--disable-werror",
                ],
                cwd=binutils_build_dir,
            )
            run_and_check(["make"], cwd=binutils_build_dir)
            run_and_check(["make", "install"], cwd=binutils_build_dir)

        if True:
            gcc_url = "https://ftp.gnu.org/gnu/gcc/gcc-11.2.0/gcc-11.2.0.tar.gz"
            gcc_dir = download_and_unpack_archive(build_dir, gcc_url)
            gcc_build_dir = build_dir / "build-gcc"
            gcc_build_dir.mkdir(exist_ok=True)
            configure_script_path = gcc_dir / "configure"
            run_and_check(
                [
                    configure_script_path.as_posix(),
                    f"--target={arch_target}",
                    f"--prefix={toolchain_dir.as_posix()}",
                    "--disable-nls",
                    "--enable-languages=c",
                    "--without-headers",
                ],
                cwd=gcc_build_dir,
            )
            run_and_check(["make", "all-gcc"], cwd=gcc_build_dir)
            run_and_check(["make", "all-target-libgcc"], cwd=gcc_build_dir)
            run_and_check(["make", "install-gcc"], cwd=gcc_build_dir)
            run_and_check(["make", "install-target-libgcc"], cwd=gcc_build_dir)

        # os.symlink("/usr/bin/grub-mkrescue", (toolchain_dir / "bin" / "grub-mkrescue").as_posix())
        os.symlink("/usr/lib/grub/i386-pc", (toolchain_dir / "lib" / "grub" / "i386-pc").as_posix())


if __name__ == "__main__":
    install()
