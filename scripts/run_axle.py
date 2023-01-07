from pathlib import Path
from build_utils import run_and_check


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
            # Capture data sent to serial port
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


if __name__ == '__main__':
    default_image_path = Path(__file__).parents[1] / "axle.iso"
    run_iso(default_image_path)
    