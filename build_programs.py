#!/usr/local/bin/python3
import os
import time
import concurrent.futures
from build_utils import run_and_check
from typing import Optional, List
from pathlib import Path


SKIP_PROGRAMS = ['cat', 'tlsclient', 'doomgeneric']


def recompile_program(program_dir: Path) -> None:
    run_and_check(['meson', 'compile', '-C', 'build'], cwd=program_dir)
    # TODO(PT): If the above command outputs "ninja: no work to do", 
    # we can skip the "install" step
    run_and_check(['meson', 'install', '-C', 'build', '--only-changed'], cwd=program_dir)


def build_all_programs(only_recently_updated: bool = False, force_rebuild_programs: Optional[List[str]] = None, force_rebuild_all=False) -> None:
    programs_root = Path(__file__).parent / "programs"
    cross_compile_config = programs_root / "cross_axle.ini"

    meson_dirs = []
    for program_dir in programs_root.iterdir():
        if not program_dir.is_dir():
            continue

        if program_dir.name in SKIP_PROGRAMS:
            continue

        if only_recently_updated and not force_rebuild_all:
            if not any([f.is_file() and os.stat(f.as_posix()).st_atime >= time.time() - 180 for f in program_dir.iterdir()]):
                # Only exclude not-recently-updated programs if not specified in programs that should be force rebuilt
                if not force_rebuild_programs:
                    continue
                if program_dir.name not in force_rebuild_programs:
                    continue
        
        meson_build_file = program_dir / 'meson.build'
        if not meson_build_file.exists():
            raise RuntimeError(f'Program not using Meson: {program_dir}')

        build_folder = program_dir / "build"

        if force_rebuild_all or (force_rebuild_programs and program_dir.name in force_rebuild_programs):
            print(f'Forcing rebuild of {program_dir}...')
            run_and_check(['rm', '-rf', build_folder.as_posix()])
        
        if not build_folder.exists():
            print(f'Running one-time Meson configuration in {program_dir}...')
            run_and_check(['meson', 'build', '--cross-file', cross_compile_config.as_posix()], cwd=program_dir)
        
        meson_dirs.append(program_dir)
        # recompile_program(program_dir)

    with concurrent.futures.ProcessPoolExecutor(max_workers=8) as executor:
        futures = [executor.submit(recompile_program, program_dir) for program_dir in meson_dirs]
    for f in futures:
        # This will raise an exception if the future raised one
        f.result()


if __name__ == '__main__':
    build_all_programs()
