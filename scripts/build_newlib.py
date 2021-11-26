#!/usr/bin/python3
import os
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
    arch = "x86_64"
    arch_target = f"{arch}-elf"
    toolchain_dir = axle_dir / f"{arch}-toolchain"
    # binaries_dir = toolchain_dir / "bin"
    binaries_dir = Path(__file__).parent / "toolchain-build-products" / "bin"

    with tempfile.TemporaryDirectory() as build_dir_raw:
        build_dir = Path(build_dir_raw)
        build_products_dir = Path(__file__).parent / "newlib-build-products"

        if True:
            automake_src_dir, automake_build_dir = clone_tool_and_prepare_build_dir(
                build_dir, "https://ftp.gnu.org/gnu/automake/automake-1.11.tar.gz"
            )
            automake_configure_path = automake_src_dir / "configure"
            run_and_check(
                [automake_configure_path.as_posix(), f"--prefix={build_products_dir}"], cwd=automake_build_dir
            )
            run_and_check(["make"], cwd=automake_build_dir)
            run_and_check(["make", "install"], cwd=automake_build_dir)

            autoconf_src_dir, autoconf_build_dir = clone_tool_and_prepare_build_dir(
                build_dir, "https://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz"
            )
            autoconf_configure_path = autoconf_src_dir / "configure"
            run_and_check(
                [autoconf_configure_path.as_posix(), f"--prefix={build_products_dir}"], cwd=autoconf_build_dir
            )
            run_and_check(["make"], cwd=autoconf_build_dir)
            run_and_check(["make", "install"], cwd=autoconf_build_dir)

        newlib_src_dir = axle_dir / "ports" / "newlib" / "newlib-2.5.0.20171222"
        newlib_build_dir = build_dir / "build-newlib"
        newlib_build_dir.mkdir()

        os.symlink((binaries_dir / f"{arch}-elf-axle-ar").as_posix(), (newlib_build_dir / f"{arch}-axle-ar").as_posix())
        os.symlink((binaries_dir / f"{arch}-elf-axle-as").as_posix(), (newlib_build_dir / f"{arch}-axle-as").as_posix())
        os.symlink((binaries_dir / f"{arch}-elf-axle-gcc").as_posix(), (newlib_build_dir / f"{arch}-axle-gcc").as_posix())
        os.symlink((binaries_dir / f"{arch}-elf-axle-cc").as_posix(), (newlib_build_dir / f"{arch}-axle-cc").as_posix())
        os.symlink((binaries_dir / f"{arch}-elf-axle-ranlib").as_posix(), (newlib_build_dir / f"{arch}-axle-ranlib").as_posix())

        env = {"PATH": f'{newlib_build_dir}:{os.environ["PATH"]}'}
        # env = {"PATH": f'{binaries_dir}:{os.environ["PATH"]}'}

        newlib_configure_path = newlib_src_dir / "configure"
        run_and_check(
            [newlib_configure_path.as_posix(), "--prefix=/usr", f"--host={arch}-axle"],
            cwd=newlib_build_dir,
            env_additions=env,
        )
        run_and_check(["make", "all"], cwd=newlib_build_dir, env_additions=env)
        run_and_check(["make", f"DESTDIR={sysroot_dir.as_posix()}", "install"], cwd=newlib_build_dir, env_additions=env)


# If you make some kind of config change to the axle target, such as adding new files within the newlib port,
# you may have to run this command
# You may see an error like the following while running this script:
# /bin/sh: /Users/philliptennen/Documents/develop/axle/ports/newlib/newlib-2.5.0.20171222/etc/configure: No such file or directory

# ../newlib-2.5.0.20171222/configure --prefix=/usr --target=i686-axle

# Fail when newlib doesn't compile
# set -e

# make all


if __name__ == "__main__":
    build()
