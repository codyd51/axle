#!/usr/local/bin/python3
from pathlib import Path

import argparse

from build_utils import run_and_check
from build_programs import build_all_programs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--force_rebuild_programs', nargs='*', action='store')
    parser.add_argument('--force_rebuild_everything', action='store_true')
    parser.set_defaults(force_rebuild_everything=False)
    args = parser.parse_args()

    parent_folder = Path(__file__).parent

    build_all_programs(only_recently_updated=True, force_rebuild_programs=args.force_rebuild_programs, force_rebuild_all=args.force_rebuild_everything)

    run_and_check(['bash', 'build-initrd-and-run.sh'], cwd=parent_folder)


if __name__ == '__main__':
    main()
