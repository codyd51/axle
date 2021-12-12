#!/usr/local/bin/python3
import os
import argparse
import tempfile
import string
import shutil
from pathlib import Path
from typing import Generator, Any
from contextlib import contextmanager

from build_kernel_headers import copy_kernel_headers
from build_utils import run_and_check, run_and_capture_output_and_check, copied_file_is_outdated
from build_programs import build_all_programs


ARCH = "x86_64"


def _is_macos() -> bool:
    return os.uname().sysname == 'Darwin'


@contextmanager
def _get_mounted_iso(image_name: Path) -> Generator[Path, Any, Any]:
    if _is_macos():
        mounted_disk_name = run_and_capture_output_and_check(
            ["hdiutil", "attach", "-imagekey", "diskimage-class=CRawDiskImage", "-nomount", image_name.as_posix()]
        ).strip(f"{string.whitespace}\n")
        print(f"Mounted disk name: {mounted_disk_name}")
        run_and_check(["newfs_msdos", "-F", "32", "-S", "512", "-s", "131072", mounted_disk_name])
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

    kernel_binary_path = Path(__file__).parents[1] / "isodir" / "boot" / "axle.bin"
    if not kernel_binary_path.exists():
        raise ValueError(f"Kernel binary missing: {kernel_binary_path}")

    initrd_path = Path(__file__).parents[1] / "isodir" / "boot" / "initrd.img"
    if not initrd_path.exists():
        raise ValueError(f"initrd missing: {initrd_path}")

    run_and_check(["dd", "if=/dev/zero", f"of={image_name.as_posix()}", "bs=512", "count=262144"])

    with _get_mounted_iso(image_name) as mount_point:
        run_and_check(["mmd", "-i", image_name.as_posix(), "::/EFI"])
        run_and_check(["mmd", "-i", image_name.as_posix(), "::/EFI/BOOT"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), bootloader_binary_path.as_posix(), "::/EFI/BOOT"])

        run_and_check(["mmd", "-i", image_name.as_posix(), "::/EFI/AXLE"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), kernel_binary_path.as_posix(), "::/EFI/AXLE/KERNEL.ELF"])
        run_and_check(["mcopy", "-i", image_name.as_posix(), initrd_path.as_posix(), "::/EFI/AXLE/INITRD.IMG"])

    return image_name


def run_iso(image_path: Path) -> None:
    # Run disk image
    run_and_check(
        [
            "qemu-system-x86_64",
            # UEFI OVMF firmware
            "-pflash",
            "/Users/philliptennen/Downloads/RELEASEX64_OVMF.fd",
            # USB containing axle disk image
            "-drive",
            f"if=none,id=usb,format=raw,file={image_path.as_posix()}",
            "-usb",
            "-device",
            "qemu-xhci,id=xhci",
            "-device",
            "usb-storage,bus=xhci.0,drive=usb",
            # Use host CPU
            "-accel",
            "hvf",
            "-cpu",
            "host",
            # Capture data sent to  serial port
            "-serial",
            "file:syslog.log",
            # SATA drive
            "-drive",
            "id=disk,file=axle-hdd.img,if=none",
            "-device",
            "ahci,id=ahci",
            "-device",
            "ide-hd,drive=disk,bus=ahci.0",
            # System configuration
            "-monitor",
            "stdio",
            "-m",
            "4G",
        ]
    )


def build_initrd() -> None:
    fsgen_path = Path(__file__).parents[1] / "fsgen"
    if not fsgen_path.exists():
        raise RuntimeError(f"fsgen binary missing, expected at {fsgen_path}")

    initrd_dir = Path(__file__).parents[1] / "initrd"
    if not initrd_dir.exists():
        raise RuntimeError(f"initrd dir missing, expected at {initrd_dir}")

    run_and_check([fsgen_path.as_posix(), "./"], cwd=initrd_dir)
    generated_initrd = initrd_dir / "initrd.img"
    if not generated_initrd.exists():
        raise RuntimeError(f"fsgen did not generate initrd at {generated_initrd}")

    staged_initrd = Path(__file__).parents[1] / "isodir" / "boot" / "initrd.img"
    shutil.move(generated_initrd.as_posix(), staged_initrd.as_posix())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--force_rebuild_programs", nargs="*", action="store")
    parser.add_argument("--force_rebuild_everything", action="store_true")
    parser.set_defaults(force_rebuild_everything=False)
    args = parser.parse_args()

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
        kernel_root / "kernel" / "pmm" / "pmm.c",
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
    run_and_check(["make", "SHELL='sh -xv'", "USE_GCC=1"], cwd=Path(__file__).parents[1] / "bootloader" / "uefi")
    run_and_check(["make"], cwd=Path(__file__).parents[1] / "bootloader")

    # Build kernel image
    run_and_check(["make"])

    # Build user programs
    build_all_programs(
        only_recently_updated=True,
        force_rebuild_programs=args.force_rebuild_programs,
        force_rebuild_all=args.force_rebuild_everything,
    )

    # Build ramdisk
    build_initrd()

    # Build disk image
    image_name = build_iso()

    run_iso(image_name)


if __name__ == "__main__":
    main()
