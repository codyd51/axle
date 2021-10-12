import os
import shutil
from pathlib import Path


def main():
    images_dir = Path(__file__).parent / "initrd_images"
    initrd_dir = Path(__file__).parent / "initrd"

    skip_images = [
        "pi_walker.bmp",
        # "record_table.bmp",
        "flips.bmp",
    ]

    # First, delete every image from the initrd folder
    for image_path in images_dir.iterdir():
        initrd_path = initrd_dir / image_path.name
        if initrd_path.exists():
            print(f"Removing {image_path.name} from initrd at {initrd_path.as_posix()}")
            os.remove(initrd_path.as_posix())

    # Now, copy every image back into initrd, excluding the images we want to ignore
    for image_path in images_dir.iterdir():
        if image_path.name in skip_images:
            continue
        if "_bak" in image_path.name:
            continue

        initrd_path = initrd_dir / image_path.name
        print(f"Copying {image_path.name} to initrd at {initrd_path.as_posix()}")
        shutil.copy(image_path.as_posix(), initrd_path.as_posix())

    # Always remove .DS_Store from initrd
    ds_store_path = initrd_dir / ".DS_Store"
    if ds_store_path.exists():
        print(f"Removing .DS_Store at {initrd_path.as_posix()}")
        os.remove(ds_store_path.as_posix())


if __name__ == "__main__":
    main()
