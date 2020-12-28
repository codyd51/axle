#!/usr/local/bin/python3
import shutil
import subprocess
import pathlib


def main():
    commands = [
        # 'build-newlib.sh',
        'python3 build-kernel-headers.py',
        'python3 build-libagx.py',
        'bash build-extern.sh',
    ]
    d = pathlib.Path(__file__).parent
    for cmd in commands:
        status = subprocess.run(cmd, cwd=d, shell=True)
        if status.returncode != 0:
            raise RuntimeError(f"{cmd} failed with exit code {status.returncode}: {status.stdout} {status.stderr}")


if __name__ == '__main__':
    main()