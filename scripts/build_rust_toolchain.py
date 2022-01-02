#!/usr/local/bin/python3
import shutil
import argparse
from pathlib import Path

from build_utils import run_and_check
from build_utils import run_and_capture_output_and_check


_REPO_ROOT = Path(__file__).parents[1]
_RUST_PROGRAMS_DIR = _REPO_ROOT / "rust_programs"
# TODO(PT): Template this file to include the full path to x86_64-elf-axle-gcc
_TARGET_SPEC_FILE = _RUST_PROGRAMS_DIR / "x86_64-unknown-axle.json"

_CACHE_DIR = Path(__file__).parent / "caches"

import tarfile
from tempfile import TemporaryDirectory


def clone_git_repo(directory: Path, url: str) -> Path:
    _CACHE_DIR.mkdir(exist_ok=True)
    repo_name = url.split('/')[-1]
    local_filename = f"{repo_name}.tar.gz"
    cached_repo = _CACHE_DIR / local_filename
    download_path = directory / local_filename

    if not cached_repo.exists():
        print(f"Downloading {url} to {download_path}...")

        with TemporaryDirectory() as temp_dir_raw:
            temp_dir = Path(temp_dir_raw)
            run_and_check(['git', 'clone', url], cwd=temp_dir)
            # .tar.gz the downloaded repo to the cache dir
            with tarfile.open(cached_repo, "w:gz") as tar:
                tar.add(temp_dir, arcname='.')

    print(f'Providing {cached_repo} from cache...')
    shutil.unpack_archive(cached_repo.as_posix(), directory.as_posix())
    repo_path = directory / repo_name
    return repo_path


def setup_rust_toolchain():
    # Build libc (C stdlib / axle syscall FFI bindings to Rust)
    with TemporaryDirectory() as temp_dir_raw:
        temp_dir = Path(temp_dir_raw)
        libc_dir = temp_dir / "libc"

        clone_git_repo(temp_dir, 'https://github.com/rust-lang/libc')
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
        # Ref: https://rustrepo.com/repo/japaric-rust-cross-rust-embedded
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
        'file_manager_messages',
        'libfs',
        'initrd_fs',
        'fs_client',
    ]
    for program_dir_name in programs:
        program_dir = _RUST_PROGRAMS_DIR / program_dir_name
        run_and_check(
            [
                'cargo',
                'fmt',
            ],
            cwd=program_dir
        )
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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rebuild_libc", action="store_true")
    parser.set_defaults(rebuild_libc=False)
    args = parser.parse_args()

    if args.rebuild_libc:
        setup_rust_toolchain()

    build_rust_programs()


if __name__ == "__main__":
    main()
