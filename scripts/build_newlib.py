#!/usr/bin/python3
import os
import shutil
import tempfile
from pathlib import Path
from typing import Tuple

from build_utils import download_and_unpack_archive, run_and_check


def clone_tool_and_prepare_build_dir(build_dir: Path, url: str) -> Tuple[Path, Path]:
    tool_src_dir = download_and_unpack_archive(build_dir, url)
    tool_name = url.split("/")[-1].removesuffix(".tar.gz")
    tool_build_dir = build_dir / f"build-{tool_name}"
    tool_build_dir.mkdir(exist_ok=True)
    return tool_src_dir, tool_build_dir


def build() -> None:
    axle_dir = Path(__file__).parents[1]
    sysroot_dir = axle_dir / "axle-sysroot"
    sysroot_dir.mkdir(exist_ok=True)
    arch = "x86_64"
    toolchain_dir = Path(__file__).parents[1] / f"{arch}-toolchain"
    binaries_dir = toolchain_dir / "bin"

    with tempfile.TemporaryDirectory() as build_dir_raw:
        build_dir = Path(build_dir_raw)
        build_products_dir = build_dir / 'build-products'

        # Build automake
        automake_src_dir, automake_build_dir = clone_tool_and_prepare_build_dir(
            build_dir, "https://ftp.gnu.org/gnu/automake/automake-1.11.tar.gz"
        )
        automake_configure_path = automake_src_dir / "configure"
        run_and_check(
            [automake_configure_path.as_posix(), f"--prefix={build_products_dir}"], cwd=automake_build_dir
        )
        run_and_check(["make"], cwd=automake_build_dir)
        run_and_check(["make", "install"], cwd=automake_build_dir)

        # Build autoconf
        autoconf_src_dir, autoconf_build_dir = clone_tool_and_prepare_build_dir(
            build_dir, "https://ftp.gnu.org/gnu/autoconf/autoconf-2.68.tar.gz"
        )
        autoconf_configure_path = autoconf_src_dir / "configure"
        run_and_check(
            [autoconf_configure_path.as_posix(), f"--prefix={build_products_dir}"], cwd=autoconf_build_dir
        )
        run_and_check(["make"], cwd=autoconf_build_dir)
        run_and_check(["make", "install"], cwd=autoconf_build_dir)

        # Build newlib
        newlib_src_dir, newlib_build_dir = clone_tool_and_prepare_build_dir(build_dir, "ftp://sourceware.org/pub/newlib/newlib-2.5.0.20171222.tar.gz")

        env = {"PATH": os.environ["PATH"]}
        # Add the x86_64-elf-axle GCC/binutils to PATH
        env = {"PATH": f'{toolchain_dir / "bin"}:{env["PATH"]}'}
        # Add autoomake and autoconf to PATH
        # Add it to the path after the x86_64-elf-axle toolchain so thee newlib-specific versions of
        # autotools are prioritised
        env = {"PATH": f'{build_products_dir / "bin"}:{env["PATH"]}'}

        newlib_patch_file = Path(__file__).parent / "newlib.patch"

        run_and_check(['git', 'apply', '--check', newlib_patch_file.as_posix()], cwd=newlib_src_dir)
        run_and_check(['git', 'apply', newlib_patch_file.as_posix()], cwd=newlib_src_dir)

        # We need to run autoconf since we modify configure.in
        run_and_check(['autoconf'], cwd=newlib_src_dir / "newlib" / "libc" / "sys", env_additions=env)
        # And autoreconf in the axle directory
        run_and_check(['autoreconf'], cwd=newlib_src_dir / "newlib" / "libc" / "sys" / "axle", env_additions=env)

        newlib_configure_path = newlib_src_dir / "configure"
        run_and_check(
            [newlib_configure_path.as_posix(), "--prefix=/usr", f"--target={arch}-elf-axle"],
            cwd=newlib_build_dir,
            env_additions=env,
        )
        run_and_check(["make", "all"], cwd=newlib_build_dir, env_additions=env)

        # Newlib builds the tree as sysroot/usr/<arch>/[include|lib]
        # gcc and binutils expect the tree to be sysroot/usr/[include|lib]
        temp_sysroot = newlib_build_dir / "sysroot"
        run_and_check(["make", f"DESTDIR={temp_sysroot.as_posix()}", "install"], cwd=newlib_build_dir, env_additions=env)
        shutil.copytree((temp_sysroot / "usr" / f'{arch}-elf-axle').as_posix(), (sysroot_dir / "usr").as_posix(), dirs_exist_ok=True)


if __name__ == "__main__":
    build()
