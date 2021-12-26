#!/usr/local/bin/python3
import shutil
from pathlib import Path

from build_utils import run_and_check
from build_utils import run_and_capture_output_and_check


_REPO_ROOT = Path(__file__).parents[1]
_RUST_PROGRAMS_DIR = _REPO_ROOT / "rust_programs"
# TODO(PT): Template this file to include the full path to x86_64-elf-axle-gcc
_TARGET_SPEC_FILE = _RUST_PROGRAMS_DIR / "x86_64-unknown-axle.json"


def setup_rust_toolchain():
    # Build libc (C stdlib / axle syscall FFI bindings to Rust)
    libc_dir = _RUST_PROGRAMS_DIR / "libc"

    run_and_check(['git', 'clone', 'https://github.com/rust-lang/libc'], cwd=_RUST_PROGRAMS_DIR)
    run_and_check(['git', 'checkout', '4b8841a38337'], cwd=libc_dir)

    libc_patch_file = Path(__file__).parent / "rust_libc.patch"
    run_and_check(['git', 'apply', '--check', libc_patch_file.as_posix()], cwd=libc_dir)
    run_and_check(['git', 'apply', libc_patch_file.as_posix()], cwd=libc_dir)

    toolchain_dir = _REPO_ROOT / 'x86_64-toolchain'
    env = {'CC': toolchain_dir / 'bin' / 'x86_64-elf-axle-gcc'}
    run_and_check(
        [
            'cargo', 
            'build', 
            '--no-default-features', 
            '-Zbuild-std=core,alloc', 
            f'--target={_TARGET_SPEC_FILE.as_posix()}'
        ], 
        cwd=libc_dir, 
        env_additions=env
    )

    # Copy the intermediate build products to the Rust 'sysroot' so they don't need to be rebuilt
    # for other projects
    rust_sysroot = Path(run_and_capture_output_and_check(['rustc', '--print', 'sysroot']).strip())
    rust_target_dir = rust_sysroot / "lib" / "rustlib" / "x86_64-unknown-axle" / "lib"
    rust_target_dir.mkdir(exist_ok=True, parents=True)
    build_dir = libc_dir / "target" / "x86_64-unknown-axle" / "debug"
    libc_build_product = build_dir / "liblibc.rlib"
    shutil.copy(libc_build_product.as_posix(), rust_target_dir.as_posix())
    for file in (build_dir / "deps").iterdir():
        if file.suffix == '.rlib':
            shutil.copy(file.as_posix(), rust_target_dir.as_posix())
    

def build_rust_programs() -> None:
    programs = [
        'axle_rt',
        'test_rust',
    ]
    for program_dir_name in programs:
        program_dir = _RUST_PROGRAMS_DIR / program_dir_name
        run_and_check(
            [
                'cargo',
                'build',
                '--release',
                f'--target={_TARGET_SPEC_FILE.as_posix()}',
            ],
            cwd=program_dir
        )
        # If this project outputs a binary, move it to /initrd/
        binary = program_dir / 'target' / 'x86_64-unknown-axle' / 'release' / program_dir_name
        if binary.exists():
            print(f'Moving build result to initrd: {binary}')
            initrd_dir = _REPO_ROOT / 'initrd'
            shutil.copy(binary.as_posix(), initrd_dir)


if __name__ == "__main__":
    #setup_rust_toolchain()
    build_rust_programs()
