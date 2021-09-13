#!/usr/local/bin/python3
from pathlib import Path

from build_utils import run_and_check
from build_programs import build_all_programs


def main():
    parent_folder = Path(__file__).parent

    build_all_programs(only_recently_updated=True)

    run_and_check(['bash', 'build-initrd-and-run.sh'], cwd=parent_folder)


if __name__ == '__main__':
    main()
