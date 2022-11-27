#!/usr/local/bin/python3
import os
import stat
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


def _install_rustc(temp_dir: Path) -> None:
    # Install Rust nightly via rustup
    rustup_install_script = temp_dir / 'rustup.sh'
    run_and_check(
        [
            "curl",
            "https://sh.rustup.rs",
            "-o",
            rustup_install_script.as_posix()
        ]
    )
    # Mark the rustup installation script as executable
    st = os.stat(rustup_install_script.as_posix())
    os.chmod(rustup_install_script.as_posix(), st.st_mode | stat.S_IEXEC)
    # Run the install script with -y to skip prompts
    run_and_check([rustup_install_script.as_posix(), "-y"])

    # Install our toolchail and required components
    run_and_check(["rustup", "install", "nightly-2022-11-27"])
    run_and_check(["rustup", "default", "nightly-2022-11-27"])
    run_and_check(["rustup", "component", "add", "rust-src"])
    run_and_check(["rustup", "component", "add", "rustfmt"])


def _build_rust_libc_port(temp_dir: Path) -> None:
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
            # '-Z macro-backtrace',
            f'--target={_TARGET_SPEC_FILE.as_posix()}',
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


def setup_rust_toolchain() -> None:
    # Build libc (C stdlib / axle syscall FFI bindings to Rust)
    with TemporaryDirectory() as temp_dir_raw:
        temp_dir = Path(temp_dir_raw)
        _install_rustc(temp_dir)
        _build_rust_libc_port(temp_dir)


def test_rust_programs() -> None:
    programs_with_tests = [
        "libfs",
        "agx_definitions",
    ]

    for program_dir_name in programs_with_tests:
        program_dir = _RUST_PROGRAMS_DIR / program_dir_name
        run_and_check(
            [
                'cargo',
                'test',
                '--release',
            ],
            cwd=program_dir
        )
    

def build_rust_programs(check_only: bool = False) -> None:
    cargo_workspace_dir = _RUST_PROGRAMS_DIR
    run_and_check(['cargo', 'fmt'], cwd=cargo_workspace_dir)
    run_and_check(
        [
            'cargo',
            'check' if check_only else 'build',
            '--release',
            f'--target={_TARGET_SPEC_FILE.as_posix()}',
            # Ref: https://users.rust-lang.org/t/cargo-features-for-host-vs-target-no-std/16911
            # https://github.com/rust-lang/cargo/issues/2589
            # https://github.com/rust-lang/cargo/issues/7915
            '-Z',
            'features=host_dep',
        ],
        cwd=cargo_workspace_dir,
        env_additions={"RUSTFLAGS": "-Cforce-frame-pointers=yes"},
    )
    for entry in cargo_workspace_dir.iterdir():
        if not entry.is_dir():
            continue

        # If this project outputs a binary, move it to /initrd/
        # Note that we look in the workspace target directory
        # binary = entry / 'target' / 'x86_64-unknown-axle' / 'release' / entry.name
        binary = cargo_workspace_dir / 'target' / 'x86_64-unknown-axle' / 'release' / entry.name
        if binary.exists():
            print(f'Moving build result to sysroot: {binary}')
            sysroot_applications_dir = _REPO_ROOT / "axle-sysroot" / "usr" / "applications"
            # Ensure that /usr/applications is treated as a directory rather than a file
            sysroot_applications_dir.mkdir(exist_ok=True)
            shutil.copy(binary.as_posix(), sysroot_applications_dir)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rebuild_libc", action="store_true")
    parser.add_argument("--test", action="store_true")
    parser.add_argument("--check_only", action="store_true")
    parser.set_defaults(rebuild_libc=False)
    parser.set_defaults(test=False)
    parser.set_defaults(check_only=False)
    args = parser.parse_args()

    if args.rebuild_libc:
        setup_rust_toolchain()
        return

    if args.test:
        test_rust_programs()
        return

    build_rust_programs(args.check_only)


if __name__ == "__main__":
    main()
