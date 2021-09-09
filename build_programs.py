#!/usr/local/bin/python3
import concurrent.futures
from build_utils import run_and_check
from pathlib import Path


SKIP_PROGRAMS = ['cat', 'tlsclient', 'doom', 'libagx']


def recompile_program(program_dir: Path) -> None:
    run_and_check(['meson', 'compile', '-C', 'build'], cwd=program_dir)
    # TODO(PT): If the above command outputs "ninja: no work to do", 
    # we can skip the "install" step
    run_and_check(['meson', 'install', '-C', 'build', '--only-changed'], cwd=program_dir)


def build_all_programs() -> None:
    programs_root = Path(__file__).parent / "programs"
    cross_compile_config = programs_root / "cross_axle.ini"

    meson_dirs = []
    for program_dir in programs_root.iterdir():
        if not program_dir.is_dir():
            continue

        if program_dir.name in SKIP_PROGRAMS:
            continue

        meson_build_file = program_dir / 'meson.build'
        if not meson_build_file.exists():
            raise RuntimeError(f'Program not using Meson: {program_dir}')
        
        build_folder = program_dir / "build"
        if not build_folder.exists():
            print(f'Running one-time Meson configuration in {program_dir}...')
            run_and_check(['meson', 'build', '--cross-file', cross_compile_config.as_posix()], cwd=program_dir)
        
        # meson_dirs.append(program_dir)
        recompile_program(program_dir)

    with concurrent.futures.ProcessPoolExecutor(max_workers=1) as executor:
        [executor.submit(recompile_program, program_dir) for program_dir in meson_dirs]


if __name__ == '__main__':
    build_all_programs()
