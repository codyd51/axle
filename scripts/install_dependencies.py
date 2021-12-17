#!/usr/bin/python3
from pathlib import Path

from build_utils import run_and_check


def install_dependencies():
    print(f"Installing dependencies...")
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


def main() -> None:
    install_dependencies()


if __name__ == '__main__':
    main()
