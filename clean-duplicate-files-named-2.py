"""I don't know why, but sometimes when working with VS Code
duplicates of many working files pop up with " 2" in the filename.
It pollutes my working index and causes problems with the build system.
This script finds and deletes them.
"""
import os
import pathlib
import shutil
import sys


def main():
    root_dir = pathlib.Path(__file__).parent
    for f in root_dir.rglob("*"):
        if " 2" in f.name:
            os.remove(f.as_posix())


if __name__ == "__main__":
    main()
