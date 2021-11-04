#!/usr/local/bin/python3
import concurrent.futures
import os
import time
from pathlib import Path
from typing import List, Optional

from build_utils import run_and_check

SKIP_PROGRAMS = ["cat", "tlsclient", "doomgeneric", "vim", "bash", "ncurses"]


def recompile_program(program_dir: Path) -> None:
    run_and_check(["meson", "compile", "-C", "build"], cwd=program_dir)
    # TODO(PT): If the above command outputs "ninja: no work to do",
    # we can skip the "install" step
    run_and_check(["meson", "install", "-C", "build", "--only-changed"], cwd=program_dir)


def build_all_programs(
    only_recently_updated: bool = False, force_rebuild_programs: Optional[List[str]] = None, force_rebuild_all=False
) -> None:
    programs_root = Path(__file__).parent / "programs"

    # https://github.com/mesonbuild/meson/issues/309
    # Since Meson won't let us fill in the repo root with an environment variable, 
    # we have to template the file ourselves...
    cross_compile_config_path = programs_root / "cross_axle_generated.ini"
    if not cross_compile_config_path.exists():
        print(f'Generating cross_axle.ini...')
        cross_compile_config_template = programs_root / "cross_axle_template.ini"
        if not cross_compile_config_template.exists():
            raise ValueError(f'Cross compile template file didn\'t exist!')
        cross_compile_config = cross_compile_config_template.read_text()
        cross_compile_config = f'[constants]\n' \
                               f'axle_repo_root = \'{Path(__file__).parent.as_posix()}\'\n' \
                               f'{cross_compile_config}'
        cross_compile_config_path.write_text(cross_compile_config)

    meson_dirs = []
    for program_dir in programs_root.iterdir():
        if not program_dir.is_dir():
            continue

        if program_dir.name in SKIP_PROGRAMS:
            continue

        if only_recently_updated and not force_rebuild_all:
            if not any(
                [f.is_file() and os.stat(f.as_posix()).st_atime >= time.time() - 180 for f in program_dir.iterdir()]
            ):
                # Only exclude not-recently-updated programs if not specified in programs that should be force rebuilt
                if not force_rebuild_programs:
                    continue
                if program_dir.name not in force_rebuild_programs:
                    continue

        meson_build_file = program_dir / "meson.build"
        if not meson_build_file.exists():
            raise RuntimeError(f"Program not using Meson: {program_dir}")

        build_folder = program_dir / "build"

        if force_rebuild_all or (force_rebuild_programs and program_dir.name in force_rebuild_programs):
            print(f"Forcing rebuild of {program_dir}...")
            run_and_check(["rm", "-rf", build_folder.as_posix()])

        if not build_folder.exists():
            print(f"Running one-time Meson configuration in {program_dir}...")
            run_and_check(["meson", "build", "--cross-file", cross_compile_config_path.as_posix()], cwd=program_dir)

        # meson_dirs.append(program_dir)
        recompile_program(program_dir)

    with concurrent.futures.ProcessPoolExecutor(max_workers=8) as executor:
        futures = [executor.submit(recompile_program, program_dir) for program_dir in meson_dirs]
    for f in futures:
        # This will raise an exception if the future raised one
        f.result()


if __name__ == "__main__":
    build_all_programs()
