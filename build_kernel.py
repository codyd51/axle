#!/usr/local/bin/python3
import shutil
from pathlib import Path

from build_kernel_headers import copy_kernel_headers
from build_programs import build_all_programs
from build_utils import run_and_check, run_and_capture_output_and_check


ARCH = "x86_64"


def main():
    # Copy architecture-specific 
    kernel_root = Path(__file__).parent / "kernel"
    arch_specific_assembly = [
        kernel_root / "boot" / "boot",
        kernel_root / "kernel" / "util" / "walk_stack",
        kernel_root / "kernel" / "multitasking" / "tasks" / "process_small",
        kernel_root / "kernel" / "segmentation" / "gdt_activate",
        kernel_root / "kernel" / "interrupts" / "idt_activate",
        kernel_root / "kernel" / "interrupts" / "int_handler_stubs",
    ]

    for file in arch_specific_assembly:
        print(f'Selecting arch-specific code for {file}...')
        final_filename = file.parent / f'{file.name}.s'
        arch_specific_file = file.parent / f'{file.name}.{ARCH}.arch_specific'

        if not arch_specific_file.exists():
            raise ValueError(f'Missing arch-specific file {arch_specific_file}')
        print(f'\tCopying {arch_specific_file} to {final_filename}...')
        shutil.copy(arch_specific_file.as_posix(), final_filename.as_posix())

    # Build kernel image
    run_and_check(['make'])
    
    # Build UEFI image
    bootloader_binary_path = Path(__file__).parent / "bootloader" / "BOOTX64.EFI"
    if not bootloader_binary_path.exists():
        raise ValueError(f'Bootloader binary missing: {bootloader_binary_path}')

    kernel_binary_path = Path(__file__).parent / "isodir" / "boot" / "axle.bin"
    if not kernel_binary_path.exists():
        raise ValueError(f'Kernel binary missing: {kernel_binary_path}')
    
    initrd_path = Path(__file__).parent / "isodir" / "boot" / "initrd.img"
    if not initrd_path.exists():
        raise ValueError(f'initrd missing: {initrd_path}')

    image_name = Path(__file__).parent / "axle.iso"
    run_and_check(['dd', 'if=/dev/zero', f'of={image_name.as_posix()}', 'bs=512', 'count=131072'])
    import string
    mounted_disk_name = run_and_capture_output_and_check(['hdiutil', 'attach', '-imagekey', 'diskimage-class=CRawDiskImage', '-nomount', image_name.as_posix()]).strip(f'{string.whitespace}\n')
    print(f'Mounted disk name: {mounted_disk_name}')
    run_and_check(['newfs_msdos', '-F', '32', '-S', '512', '-s', '131072', mounted_disk_name])
    run_and_check(['mmd', '-i', image_name.as_posix(), '::/EFI'])
    run_and_check(['mmd', '-i', image_name.as_posix(), '::/EFI/BOOT'])
    run_and_check(['mcopy', '-i', image_name.as_posix(), bootloader_binary_path.as_posix(), '::/EFI/BOOT'])

    run_and_check(['mmd', '-i', image_name.as_posix(), '::/EFI/AXLE'])
    run_and_check(['mcopy', '-i', image_name.as_posix(), kernel_binary_path.as_posix(), '::/EFI/AXLE/KERNEL.ELF'])
    #run_and_check(['mcopy', '-i', image_name.as_posix(), initrd_path.as_posix(), '::/EFI/AXLE/INITRD.IMG'])
    run_and_check(['mcopy', '-i', image_name.as_posix(), kernel_binary_path.as_posix(), '::/EFI/AXLE/INITRD.IMG'])

    run_and_check(['qemu-system-x86_64', '-bios', '/Users/philliptennen/Downloads/RELEASEX64_OVMF.fd', '-usb', '-drive', f'if=none,id=stick,format=raw,file={image_name.as_posix()}', '-device', 'usb-storage,drive=stick', '-monitor', 'stdio', '-m', '2G', ' -serial file\:syslog.log'])


if __name__ == "__main__":
    main()
