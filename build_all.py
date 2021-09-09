#!/usr/local/bin/python3
from pathlib import Path

from build_utils import run_and_check
from build_kernel_headers import copy_kernel_headers
from build_programs import build_all_programs


def main():
    parent_folder = Path(__file__).parent
    # run_and_check(['bash', 'build-newlib.sh'], cwd=parent_folder)

    copy_kernel_headers()
    build_all_programs()

    run_and_check(['bash', 'build-initrd-and-run.sh'], cwd=parent_folder)


if __name__ == '__main__':
    main()