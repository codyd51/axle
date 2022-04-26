#!/usr/local/bin/python3
import argparse
from pathlib import Path
from typing import List

from build_utils import run_and_check
from build_userspace_headers import generate_meson_cross_file_if_necessary, copy_userspace_headers


def build_meson_projects(only_build: List[str]) -> None:
    copy_userspace_headers()

    programs_root = Path(__file__).parents[1] / "programs"

    build_folder = programs_root / "build"
    force_rebuild_all = False
    if force_rebuild_all:
        print(f"Forcing rebuild of {programs_root}...")
        run_and_check(["rm", "-rf", build_folder.as_posix()])

    if not build_folder.exists():
        print(f"Running one-time Meson configuration in {programs_root}...")
        cross_compile_config_path = generate_meson_cross_file_if_necessary()
        run_and_check(["meson", "build", "--cross-file", cross_compile_config_path.as_posix()], cwd=programs_root)

    if only_build:
        for subdir_name in only_build:
            subdir = programs_root / "subprojects" / subdir_name
            run_and_check(["meson", "compile"], cwd=subdir)
            run_and_check(["meson", "install"], cwd=programs_root)
    else:
        run_and_check(["meson", "compile", "-C", "build"], cwd=programs_root)
        run_and_check(["meson", "install", "-C", "build", "--only-changed"], cwd=programs_root)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--only_build", nargs="*", action="store")
    args = parser.parse_args()

    build_meson_projects(args.only_build)
