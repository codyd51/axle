#!/usr/local/bin/python3
"""Install public kernel headers into /usr/include/
These are functions available via syscalls to user-mode programs.
"""
import shutil
from distutils.dir_util import copy_tree
from pathlib import Path

from build_utils import copied_file_is_outdated


def copy_kernel_headers():
    arch = "x86_64"

    src_root = Path(__file__).parents[1] / "kernel"
    bootloader_root = Path(__file__).parents[1] / "bootloader"
    sysroot = Path(__file__).parents[1] / "axle-sysroot"
    include_dir = sysroot / "usr" / "include"
    programs_root = Path(__file__).parents[1] / "programs"

    headers_to_copy = [
        # Copy kernel headers to the sysroot
        (src_root / "kernel" / "util" / "amc" / "amc.h", include_dir / "kernel" / "amc.h"),
        (src_root / "kernel" / "util" / "amc" / "core_commands.h", include_dir / "kernel" / "core_commands.h"),
        (src_root / "kernel" / "util" / "adi" / "adi.h", include_dir / "kernel" / "adi.h"),
        (src_root / "kernel" / "interrupts" / "idt.h", include_dir / "kernel" / "idt.h"),
        # Copy bootloader header to the sysroot
        (bootloader_root / "axle_boot_info.h", include_dir / "bootloader" / "axle_boot_info.h"),

        # Copy user services headers to the sysroot
        # PT: Note that these paths are duplicated from the install_headers() rule in each Meson subproject.
        # They're included here because on the first sysroot build, there are cyclic dependencies between various programs'
        # message definitions. We copy them here outside of meson to handle this, but the downside is that this list must
        # be manually kept up-to-date. Daisy says hello!
        (programs_root / "ata_driver" / "ata_driver_messages.h", include_dir / "drivers" / "ata" / "ata_driver_messages.h"),

        (programs_root / "libagx" / "lib" / "point.h", include_dir / "agx" / "lib" / "point.h"),
        (programs_root / "libagx" / "lib" / "color.h", include_dir / "agx" / "lib" / "color.h"),
        (programs_root / "libagx" / "lib" / "putpixel.h", include_dir / "agx" / "lib" / "putpixel.h"),
        (programs_root / "libagx" / "lib" / "size.h", include_dir / "agx" / "lib" / "size.h"),
        (programs_root / "libagx" / "lib" / "ca_layer.h", include_dir / "agx" / "lib" / "ca_layer.h"),
        (programs_root / "libagx" / "lib" / "shapes.h", include_dir / "agx" / "lib" / "shapes.h"),
        (programs_root / "libagx" / "lib" / "rect.h", include_dir / "agx" / "lib" / "rect.h"),
        (programs_root / "libagx" / "lib" / "text_box.h", include_dir / "agx" / "lib" / "text_box.h"),
        (programs_root / "libagx" / "lib" / "elem_stack.h", include_dir / "agx" / "lib" / "elem_stack.h"),
        (programs_root / "libagx" / "lib" / "hash_map.h", include_dir / "agx" / "lib" / "hash_map.h"),
        (programs_root / "libagx" / "lib" / "screen.h", include_dir / "agx" / "lib" / "screen.h"),
        (programs_root / "libagx" / "lib" / "gfx.h", include_dir / "agx" / "lib" / "gfx.h"),

        (programs_root / "libagx" / "font" / "font.h", include_dir / "agx" / "font" / "font.h"),
        (programs_root / "libagx" / "font" / "font8x8.h", include_dir / "agx" / "font" / "font8x8.h"),

        (programs_root / "libamc" / "libamc.h", include_dir / "libamc" / "libamc.h"),

        (programs_root / "libfiles" / "libfiles.h", include_dir / "libfiles" / "libfiles.h"),

        (programs_root / "libgui" / "gui_elem.h", include_dir / "libgui" / "gui_elem.h"),
        (programs_root / "libgui" / "gui_button.h", include_dir / "libgui" / "gui_button.h"),
        (programs_root / "libgui" / "gui_layer.h", include_dir / "libgui" / "gui_layer.h"),
        (programs_root / "libgui" / "gui_scroll_view.h", include_dir / "libgui" / "gui_scroll_view.h"),
        (programs_root / "libgui" / "gui_scrollbar.h", include_dir / "libgui" / "gui_scrollbar.h"),
        (programs_root / "libgui" / "gui_slider.h", include_dir / "libgui" / "gui_slider.h"),
        (programs_root / "libgui" / "gui_text_input.h", include_dir / "libgui" / "gui_text_input.h"),
        (programs_root / "libgui" / "gui_text_view.h", include_dir / "libgui" / "gui_text_view.h"),
        (programs_root / "libgui" / "gui_timer.h", include_dir / "libgui" / "gui_timer.h"),
        (programs_root / "libgui" / "gui_view.h", include_dir / "libgui" / "gui_view.h"),
        (programs_root / "libgui" / "libgui.h", include_dir / "libgui" / "libgui.h"),
        (programs_root / "libgui" / "utils.h", include_dir / "libgui" / "utils.h"),

        (programs_root / "libimg" / "libimg.h", include_dir / "libimg" / "libimg.h"),
        (programs_root / "libnet" / "libnet.h", include_dir / "libnet" / "libnet.h"),
        (programs_root / "libport" / "libport.h", include_dir / "libport" / "libport.h"),

        (programs_root / "awm" / "awm.h", include_dir / "awm" / "awm.h"),
        (programs_root / "awm" / "awm_messages.h", include_dir / "awm" / "awm_messages.h"),
        (programs_root / "crash_reporter" / "crash_reporter_messages.h", include_dir / "crash_reporter" / "crash_reporter_messages.h"),
        (programs_root / "file_manager" / "file_manager_messages.h", include_dir / "file_manager" / "file_manager_messages.h"),
        (programs_root / "image_viewer" / "image_viewer_messages.h", include_dir / "image_viewer" / "image_viewer_messages.h"),
        (programs_root / "kb_driver" / "kb_driver_messages.h", include_dir / "drivers" / "kb" / "kb_driver_messages.h"),
        (programs_root / "net" / "net_messages.h", include_dir / "net" / "net_messages.h"),
        (programs_root / "pci_driver" / "pci_messages.h", include_dir / "pci" / "pci_messages.h"),
        (programs_root / "preferences" / "preferences_messages.h", include_dir / "preferences" / "preferences_messages.h"),
        (programs_root / "realtek_8139_driver" / "rtl8139_messages.h", include_dir / "drivers" / "realtek_8139" / "realtek_8139_messages.h"),
        (programs_root / "watchdogd" / "watchdogd_messages.h", include_dir / "daemons" / "watchdogd" / "watchdogd_messages.h"),
    ]

    for source_path, include_path in headers_to_copy:
        include_path.parent.mkdir(exist_ok=True, parents=True)
        # If the files are identical, no need to copy
        if not include_path.exists() or copied_file_is_outdated(source_path, include_path):
            print(f"Copying kernel source tree {source_path} to sysroot path {include_path}")
            if source_path.is_dir():
                copy_tree(source_path.as_posix(), include_path.as_posix())
            else:
                shutil.copy(source_path.as_posix(), include_path.as_posix())
    
    programs_root = Path(__file__).parents[1] / "programs"


if __name__ == "__main__":
    copy_kernel_headers()
