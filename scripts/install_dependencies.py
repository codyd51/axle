#!/usr/bin/python3
import sys

from build_utils import run_and_check


def install_dependencies_on_linux():
    run_and_check(["sudo", "apt", "update"])
    dependencies = [
        # Toolchain
        "build-essential",
        "bison",
        "flex",
        "libgmp3-dev",
        "libmpc-dev",
        "libmpfr-dev",
        "texinfo",
        "xorriso",
        # OS build
        "nasm",
        "mtools"
    ]
    run_and_check(["sudo", "apt", "install", "-y", *dependencies])

    # requirements_file = Path(__file__).parents[1] / "python-dependencies.txt"
    # run_and_check(["pip3", "install",  "-r", requirements_file.as_posix()])


def install_dependencies_on_macos():
    dependencies = [
        # Toolchain
        "bison",
        "gmp",
        "mpc",
        "libmpc",
        "mpfr",
        "xorriso",
        "autoconf@2.69",
        # OS build
        "mtools",
    ]
    run_and_check(["brew", "install", *dependencies])


def main() -> None:
    print(f"Installing dependencies...")
    if sys.platform == 'darwin':
        install_dependencies_on_macos()
    else:
        install_dependencies_on_linux()


if __name__ == '__main__':
    main()
