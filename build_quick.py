#!/usr/local/bin/python3
import shutil
import argparse
from pathlib import Path

from build_programs import build_all_programs
from build_kernel import build_iso, build_initrd, run_iso


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--force_rebuild_programs", nargs="*", action="store")
    parser.add_argument("--force_rebuild_everything", action="store_true")
    parser.set_defaults(force_rebuild_everything=False)
    args = parser.parse_args()

    build_all_programs(
        only_recently_updated=True,
        force_rebuild_programs=args.force_rebuild_programs,
        force_rebuild_all=args.force_rebuild_everything,
    )

    build_initrd()
    image_path = build_iso()
    run_iso(image_path)


if __name__ == "__main__":
    main()
