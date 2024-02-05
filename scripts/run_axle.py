from pathlib import Path
from build_utils import run_and_check


def run_iso(image_path: Path, debug_with_gdb: bool = False) -> None:
    extra_args = (
        [
            # Use host CPU
            # Not compatible with GDB
            # PT: Also not compatible with AArch64 Macs, as axle targets x86_64.
            # TODO(PT): Conditionally enable these flags if we're running on an x86_64 host (and select accelerator appropriately).
            # "-accel",
            # "hvf",
            # "-cpu",
            # "host",
        ]
        if not debug_with_gdb else [
            "-s", '-S',
        ]
    )
    # Run disk image
    run_and_check(
        [
            # PT: Run natively on M1 Macs
            # TODO(PT): Support a wider variety of host configurations out-of-the-box
            "arch",
            "-arm64",
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
            # Capture data sent to serial port
            "-serial",
            "file:syslog.log",
            # SATA drive
            # "-drive",
            # "id=disk,file=axle-hdd.img,if=none",
            # "-device",
            # "ahci,id=ahci",
            # "-device",
            # "ide-hd,drive=disk,bus=ahci.0",
            # System configuration
            "-monitor",
            "stdio",
            "-m",
            "4G",
            # "-full-screen",
            "-vga",
            "virtio",
            # Use multiple CPU cores
            #"-smp",
            #"4",
            "-net","nic,model=rtl8139",
            #"-netdev vmnet-shared,id=net0 -device virtio-net-device,netdev=net0",
            # "-netdev vmnet-shared,id=vmnet -device rtl8139,netdev=vmnet",
        ] + extra_args
    )


if __name__ == '__main__':
    default_image_path = Path(__file__).parents[1] / "axle.iso"
    run_iso(default_image_path)
    
