import subprocess
from pathlib import Path
from build_utils import run_and_check


def main():
    volume = Path('/Volumes/NO\\ NAME/')
    print(volume)
    #assert volume.exists()
    kernel_path = volume / 'EFI' / 'AXLE' / 'KERNEL.ELF'
    initrd_path = volume / 'EFI' / 'AXLE' / 'INITRD.IMG'
    bootloader_path = volume / 'EFI' / 'BOOT' / 'BOOTX64.EFI'

    isodir = Path(__file__).parent / 'isodir'
    #assert isodir.exists()
    kernel_source_path = isodir / 'boot' / 'axle.bin'
    initrd_source_path = isodir / 'boot' / 'initrd.img'
    bootloader_source_path = Path(__file__).parent / "bootloader" / "BOOTX64.EFI"

    print(f'cp {kernel_source_path.as_posix()} {kernel_path.as_posix()}')
    status = subprocess.run(f'cp {kernel_source_path.as_posix()} {kernel_path.as_posix()}', shell=True)
    print(f'cp {initrd_source_path.as_posix()} {initrd_path.as_posix()}')
    status = subprocess.run(f'cp {initrd_source_path.as_posix()} {initrd_path.as_posix()}', shell=True)
    print(f'cp {bootloader_source_path.as_posix()} {bootloader_path.as_posix()}')
    status = subprocess.run(f'cp {bootloader_source_path.as_posix()} {bootloader_path.as_posix()}', shell=True)


if __name__ == '__main__':
    main()
