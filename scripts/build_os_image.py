#!/usr/local/bin/python3
import argparse
import tempfile
import string
import shutil
from pathlib import Path
from typing import Generator, Any
from contextlib import contextmanager

from build_kernel_headers import copy_kernel_headers
from build_utils import run_and_check, run_and_capture_output_and_check, copied_file_is_outdated, is_on_macos
from build_meson_projects import build_meson_projects
from build_rust_toolchain import build_rust_programs, build_kernel_rust_libs
from run_axle import run_iso


ARCH = "x86_64"

_REPO_ROOT = Path(__file__).parents[1]


@contextmanager
def _get_mounted_iso(image_name: Path) -> Generator[Path, Any, Any]:
    disk_size_in_mb = 128
    sector_size = 512
    sector_count = (disk_size_in_mb * 1024 * 1024) / sector_size
    if is_on_macos():
        mounted_disk_name = run_and_capture_output_and_check(
            ["hdiutil", "attach", "-imagekey", "diskimage-class=CRawDiskImage", "-nomount", image_name.as_posix()]
        ).strip(f"{string.whitespace}\n")
        print(f"Mounted disk name: {mounted_disk_name}")
        run_and_check(["newfs_msdos", "-F", "32", "-S", str(sector_size), "-s", str(int(sector_count)), mounted_disk_name])
        yield Path(mounted_disk_name)
    else:
        run_and_check(['mkfs.vfat', image_name.as_posix()])
        with tempfile.TemporaryDirectory() as temp_dir:
            mount_point = Path(temp_dir) / "mnt"
            mount_point.mkdir()
            run_and_check(['sudo', 'mount', '-o', 'loop', image_name.as_posix(), mount_point.as_posix()])
            yield mount_point
            run_and_check(['sudo', 'umount', image_name.as_posix()])


def build_iso() -> Path:
    image_name = Path(__file__).parents[1] / "axle.iso"
    bootloader_binary_path = Path(__file__).parents[1] / "bootloader" / "BOOTX64.EFI"
    if not bootloader_binary_path.exists():
        raise ValueError(f"Bootloader binary missing: {bootloader_binary_path}")

    kernel_binary_path = _REPO_ROOT / "isodir" / "boot" / "axle.bin"
    if not kernel_binary_path.exists():
        raise ValueError(f"Kernel binary missing: {kernel_binary_path}")

    fs_server_path = _REPO_ROOT / "axle-sysroot" / "usr" / "applications" / "initrd_fs"
    if not fs_server_path.exists():
        raise ValueError(f"fs_server missing: {fs_server_path}")

    initrd_path = _REPO_ROOT / "isodir" / "boot" / "initrd.img"
    if not initrd_path.exists():
        raise ValueError(f"initrd missing: {initrd_path}")

    ap_bootstrap_path = _REPO_ROOT / ".compiled_ap_bootstrap"
    if not ap_bootstrap_path.exists():
        raise ValueError(f"AP bootstrap missing: {ap_bootstrap_path}")

    run_and_check(["dd", "if=/dev/zero", f"of={image_name.as_posix()}", "bs=512", "count=262144"])

    with _get_mounted_iso(image_name) as mount_point:
        run_and_check(["mmd", "-i", image_name.as_posix(), "::/EFI"])
        run_and_check(["mmd", "-i", image_name.as_posix(), "::/EFI/BOOT"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), bootloader_binary_path.as_posix(), "::/EFI/BOOT"])

        run_and_check(["mmd", "-i", image_name.as_posix(), "::/EFI/AXLE"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), kernel_binary_path.as_posix(), "::/EFI/AXLE/KERNEL.ELF"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), fs_server_path.as_posix(), "::/EFI/AXLE/FS_SERVER.ELF"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), initrd_path.as_posix(), "::/EFI/AXLE/INITRD.IMG"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), ap_bootstrap_path.as_posix(), "::/EFI/AXLE/AP_BOOTSTRAP.BIN"])

    return image_name


def build_initrd() -> None:
    mkinitrd_path = Path(__file__).parent / "mkinitrd"
    if not mkinitrd_path.exists():
        raise RuntimeError(f"mkinitrd directory missing, expected at {mkinitrd_path}")
    
    # This will also build mkinitrd, if necessary
    run_and_check(['cargo', 'run', '--release'], cwd=mkinitrd_path)
    # TODO(PT): How to select whether to build or not?
    # run_and_check(["./target/release/mkinitrd"], cwd=mkinitrd_path)

    generated_initrd = mkinitrd_path / "output.img"
    if not generated_initrd.exists():
        raise RuntimeError(f"mkinitrd did not generate initrd at {generated_initrd}")

    staged_initrd = Path(__file__).parents[1] / "isodir" / "boot" / "initrd.img"
    shutil.copy(generated_initrd.as_posix(), staged_initrd.as_posix())


def build_dist_tree() -> None:
    dist_folder = Path(__file__).parents[1] / "os_dist"
    sysroot = Path(__file__).parents[1] / "axle-sysroot"
    for path in dist_folder.rglob("*"):
        if path.name == '.DS_Store':
            continue

        relative_to_root = path.relative_to(dist_folder)
        sysroot_path = sysroot / relative_to_root

        if path.is_dir():
            sysroot_path.mkdir(exist_ok=True)
            continue

        if not sysroot_path.exists() or copied_file_is_outdated(path, sysroot_path):
            print(f'Copying {path} to {sysroot_path}')
            shutil.copy(path.as_posix(), sysroot_path.as_posix())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--force_rebuild_programs", nargs="*", action="store")
    parser.add_argument("--force_rebuild_everything", action="store_true")
    parser.add_argument("--no_run", action="store_true")
    parser.add_argument("--run_only", action="store_true")
    parser.add_argument("--debug", action="store_true")
    parser.set_defaults(force_rebuild_everything=False)
    parser.set_defaults(no_run=False)
    parser.set_defaults(debug=False)
    args = parser.parse_args()

    if args.run_only:
        image_name = Path(__file__).parents[1] / "axle.iso"
        run_iso(image_name, debug_with_gdb=args.debug)
        return

    # Stage kernel headers
    copy_kernel_headers()

    # Stage architecture-specific source files
    kernel_root = Path(__file__).parents[1] / "kernel"
    arch_specific_assembly = [
        kernel_root / "boot" / "boot.s",
        kernel_root / "kernel" / "util" / "walk_stack.s",
        kernel_root / "kernel" / "multitasking" / "tasks" / "process_small.s",
        kernel_root / "kernel" / "segmentation" / "gdt_activate.s",
        kernel_root / "kernel" / "interrupts" / "idt_activate.s",
        kernel_root / "kernel" / "interrupts" / "int_handler_stubs.s",
        kernel_root / "kernel" / "pmm" / "pmm_int.h",
        kernel_root / "kernel" / "vmm" / "vmm.h",
        kernel_root / "kernel" / "vmm" / "vmm.c",
    ]

    for file in arch_specific_assembly:
        arch_specific_file = file.parent / f"{file.name}.{ARCH}.arch_specific"

        if not arch_specific_file.exists():
            raise ValueError(f"Missing arch-specific file {arch_specific_file}")

        if not file.exists() or copied_file_is_outdated(arch_specific_file, file):
            print(f"\tCopying arch-specific code {arch_specific_file} to {file}...")
            shutil.copy(arch_specific_file.as_posix(), file.as_posix())

    # Build bootloader
    if is_on_macos():
        env = {}
    else:
        env = {"USE_GCC": "1", "SHELL": "sh -xv"}
    #run_and_check(["make"], cwd=_REPO_ROOT / "bootloader" / "uefi", env_additions=env)
    run_and_check(["make"], cwd=_REPO_ROOT / "bootloader", env_additions=env)

    # Build the Rust kernel libraries
    build_kernel_rust_libs()

    # Build the AP bootstrap, which needs to be outside the kernel proper
    # TODO(PT): Render this based on the target architecture
    run_and_check(["nasm", "-f", "bin", (_REPO_ROOT / "ap_bootstrap.s.x86_64.arch_specific").as_posix(), "-o", (_REPO_ROOT / ".compiled_ap_bootstrap").as_posix()])

    # Build the C and assembly portions of the kernel, and link with the Rust libraries
    run_and_check(["make"], cwd=_REPO_ROOT)

    working_on_kernel_only = False
    if not working_on_kernel_only:
        # Build Rust programs before C programs as the C programs might
        # need headers installed by Rust build scripts
        build_rust_programs()

        # Build user programs
        build_meson_projects()

        build_dist_tree()

        # Build ramdisk
        build_initrd()

    # Build disk image
    image_name = build_iso()
    
    # Copy kernel to USB
    # USB available?
    usb_root = Path("/Volumes/NO NAME")
    if usb_root.exists():
        kernel_path = usb_root / "EFI" / "AXLE" / "KERNEL.ELF"
        fs_server_path = usb_root / "EFI" / "AXLE" / "FS_SERVER.ELF"
        initrd_path = usb_root / "EFI" / "AXLE" / "INITRD.IMG"
        if kernel_path.exists() and fs_server_path.exists():
            kernel_src = Path(__file__).parents[1] / "isodir" / "boot" / "axle.bin"
            fs_server_src = Path(__file__).parents[1] / "axle-sysroot" / "usr" / "applications" / "initrd_fs"
            initrd_src = Path(__file__).parents[1] / "isodir" / "boot" / "initrd.img"
            print("\n\n\nCopying kernel to USB\n\n\n")
            shutil.copy(kernel_src.as_posix(), kernel_path.as_posix())
            shutil.copy(fs_server_src.as_posix(), fs_server_path.as_posix())
            shutil.copy(initrd_src.as_posix(), initrd_path.as_posix())
        else:
            print("\n\n\nUnexpected USB tree, will not update image\n\n\n")
    else:
        print("\n\n\nNo USB detected, will not update image\n\n\n")

    if not args.no_run:
        run_iso(image_name, args.debug)
    

if __name__ == "__main__":
    main()
